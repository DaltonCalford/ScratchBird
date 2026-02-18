/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/types.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace scratchbird::core
{
    namespace
    {
        struct EmulationTypeRow
        {
            const char* engine_name;
            const char* emulated_type;
            EmulatedStorageKind storage_kind;
            DataType canonical_type;
            const char* domain_hint;
            const char* parser_rule_hint;
        };

        std::string trimAscii(const std::string& input)
        {
            size_t start = 0;
            while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])))
            {
                ++start;
            }
            size_t end = input.size();
            while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])))
            {
                --end;
            }
            return input.substr(start, end - start);
        }

        std::string uppercaseAscii(std::string input)
        {
            std::transform(input.begin(), input.end(), input.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            return input;
        }

        std::string normalizeSpaces(const std::string& input)
        {
            std::string out;
            out.reserve(input.size());
            bool prev_space = false;
            for (char c : input)
            {
                if (std::isspace(static_cast<unsigned char>(c)))
                {
                    if (!prev_space)
                    {
                        out.push_back(' ');
                        prev_space = true;
                    }
                    continue;
                }
                out.push_back(c);
                prev_space = false;
            }
            return trimAscii(out);
        }

        std::string normalizeEngineName(const std::string& engine_name)
        {
            std::string normalized = uppercaseAscii(trimAscii(engine_name));
            if (normalized == "FIREBIRDSQL")
            {
                return "FIREBIRD";
            }
            if (normalized == "MARIADB")
            {
                // MariaDB emulation currently reuses the MySQL datatype matrix.
                return "MYSQL";
            }
            if (normalized == "POSTGRES")
            {
                return "POSTGRESQL";
            }
            if (normalized == "MONGO")
            {
                return "MONGODB";
            }
            return normalized;
        }

        std::string normalizeEmulatedTypeName(const std::string& type_name)
        {
            std::string normalized = uppercaseAscii(normalizeSpaces(type_name));
            const size_t paren_pos = normalized.find('(');
            if (paren_pos != std::string::npos)
            {
                normalized = trimAscii(normalized.substr(0, paren_pos));
            }
            const size_t generic_pos = normalized.find('<');
            if (generic_pos != std::string::npos)
            {
                normalized = trimAscii(normalized.substr(0, generic_pos));
            }
            if (normalized == "CHARACTER VARYING")
            {
                return "VARCHAR";
            }
            if (normalized == "CHARACTER")
            {
                return "CHAR";
            }
            return normalized;
        }

        const std::vector<EmulationTypeRow> kEmulationTypeMatrix{
            // Firebird scalar/complex
            {"FIREBIRD", "SMALLINT", EmulatedStorageKind::NATIVE, DataType::INT16, "", "direct"},
            {"FIREBIRD", "INTEGER", EmulatedStorageKind::NATIVE, DataType::INT32, "", "direct"},
            {"FIREBIRD", "INT", EmulatedStorageKind::NATIVE, DataType::INT32, "", "direct"},
            {"FIREBIRD", "BIGINT", EmulatedStorageKind::NATIVE, DataType::INT64, "", "direct"},
            {"FIREBIRD", "INT128", EmulatedStorageKind::NATIVE, DataType::INT128, "", "direct"},
            {"FIREBIRD", "NUMERIC", EmulatedStorageKind::NATIVE, DataType::DECIMAL, "", "precision/scale"},
            {"FIREBIRD", "DECIMAL", EmulatedStorageKind::NATIVE, DataType::DECIMAL, "", "precision/scale"},
            {"FIREBIRD", "DECFLOAT16", EmulatedStorageKind::NATIVE, DataType::DECFLOAT16, "", "direct"},
            {"FIREBIRD", "DECFLOAT34", EmulatedStorageKind::NATIVE, DataType::DECFLOAT34, "", "direct"},
            {"FIREBIRD", "REAL", EmulatedStorageKind::NATIVE, DataType::FLOAT32, "", "direct"},
            {"FIREBIRD", "DOUBLE PRECISION", EmulatedStorageKind::NATIVE, DataType::FLOAT64, "", "direct"},
            {"FIREBIRD", "BOOLEAN", EmulatedStorageKind::NATIVE, DataType::BOOLEAN, "", "direct"},
            {"FIREBIRD", "DATE", EmulatedStorageKind::NATIVE, DataType::DATE, "", "direct"},
            {"FIREBIRD", "TIME", EmulatedStorageKind::NATIVE, DataType::TIME, "", "direct"},
            {"FIREBIRD", "TIME WITH TIME ZONE", EmulatedStorageKind::NATIVE, DataType::TIME_WITH_ZONE, "", "direct"},
            {"FIREBIRD", "TIMESTAMP", EmulatedStorageKind::NATIVE, DataType::TIMESTAMP, "", "direct"},
            {"FIREBIRD", "TIMESTAMP WITH TIME ZONE", EmulatedStorageKind::NATIVE, DataType::TIMESTAMP_WITH_ZONE, "", "direct"},
            {"FIREBIRD", "CHAR", EmulatedStorageKind::NATIVE, DataType::CHAR, "", "length chars"},
            {"FIREBIRD", "VARCHAR", EmulatedStorageKind::NATIVE, DataType::VARCHAR, "", "length chars"},
            {"FIREBIRD", "BINARY", EmulatedStorageKind::DOMAIN, DataType::BINARY, "[sb_fb_dom]binary_octets", "octets charset"},
            {"FIREBIRD", "BLOB", EmulatedStorageKind::NATIVE, DataType::BLOB, "", "subtype controls text/binary"},
            {"FIREBIRD", "UUID", EmulatedStorageKind::NATIVE, DataType::UUID, "", "direct"},
            {"FIREBIRD", "ARRAY", EmulatedStorageKind::NATIVE, DataType::ARRAY, "", "array bounds"},
            {"FIREBIRD", "DOMAIN", EmulatedStorageKind::NATIVE, DataType::DOMAIN, "", "domain constraints"},
            {"FIREBIRD", "USER-DEFINED TYPE", EmulatedStorageKind::NATIVE, DataType::COMPOSITE, "", "composite fields"},

            // PostgreSQL scalar/complex
            {"POSTGRESQL", "SMALLINT", EmulatedStorageKind::NATIVE, DataType::INT16, "", "direct"},
            {"POSTGRESQL", "INTEGER", EmulatedStorageKind::NATIVE, DataType::INT32, "", "direct"},
            {"POSTGRESQL", "BIGINT", EmulatedStorageKind::NATIVE, DataType::INT64, "", "direct"},
            {"POSTGRESQL", "SMALLSERIAL", EmulatedStorageKind::DOMAIN, DataType::INT16, "[sb_pg_dom]smallserial", "identity sequence"},
            {"POSTGRESQL", "SERIAL", EmulatedStorageKind::DOMAIN, DataType::INT32, "[sb_pg_dom]serial", "identity sequence"},
            {"POSTGRESQL", "BIGSERIAL", EmulatedStorageKind::DOMAIN, DataType::INT64, "[sb_pg_dom]bigserial", "identity sequence"},
            {"POSTGRESQL", "NUMERIC", EmulatedStorageKind::NATIVE, DataType::DECIMAL, "", "precision/scale"},
            {"POSTGRESQL", "DECIMAL", EmulatedStorageKind::NATIVE, DataType::DECIMAL, "", "precision/scale"},
            {"POSTGRESQL", "REAL", EmulatedStorageKind::NATIVE, DataType::FLOAT32, "", "direct"},
            {"POSTGRESQL", "DOUBLE PRECISION", EmulatedStorageKind::NATIVE, DataType::FLOAT64, "", "direct"},
            {"POSTGRESQL", "MONEY", EmulatedStorageKind::DOMAIN, DataType::MONEY, "[sb_pg_dom]money", "money_scale=2"},
            {"POSTGRESQL", "BOOLEAN", EmulatedStorageKind::NATIVE, DataType::BOOLEAN, "", "direct"},
            {"POSTGRESQL", "CHAR", EmulatedStorageKind::NATIVE, DataType::CHAR, "", "length chars"},
            {"POSTGRESQL", "VARCHAR", EmulatedStorageKind::NATIVE, DataType::VARCHAR, "", "length chars"},
            {"POSTGRESQL", "TEXT", EmulatedStorageKind::NATIVE, DataType::TEXT, "", "direct"},
            {"POSTGRESQL", "NAME", EmulatedStorageKind::DOMAIN, DataType::CHAR, "[sb_pg_dom]name", "char(64)"},
            {"POSTGRESQL", "BYTEA", EmulatedStorageKind::NATIVE, DataType::BYTEA, "", "direct"},
            {"POSTGRESQL", "BIT", EmulatedStorageKind::DOMAIN, DataType::BIT, "[sb_pg_dom]bit", "fixed bit length"},
            {"POSTGRESQL", "UUID", EmulatedStorageKind::NATIVE, DataType::UUID, "", "direct"},
            {"POSTGRESQL", "DATE", EmulatedStorageKind::NATIVE, DataType::DATE, "", "direct"},
            {"POSTGRESQL", "TIME", EmulatedStorageKind::NATIVE, DataType::TIME, "", "direct"},
            {"POSTGRESQL", "TIME WITH TIME ZONE", EmulatedStorageKind::NATIVE, DataType::TIME_WITH_ZONE, "", "direct"},
            {"POSTGRESQL", "TIMESTAMP", EmulatedStorageKind::NATIVE, DataType::TIMESTAMP, "", "direct"},
            {"POSTGRESQL", "TIMESTAMP WITH TIME ZONE", EmulatedStorageKind::NATIVE, DataType::TIMESTAMP_WITH_ZONE, "", "direct"},
            {"POSTGRESQL", "INTERVAL", EmulatedStorageKind::NATIVE, DataType::INTERVAL, "", "direct"},
            {"POSTGRESQL", "INET", EmulatedStorageKind::NATIVE, DataType::INET, "", "direct"},
            {"POSTGRESQL", "CIDR", EmulatedStorageKind::NATIVE, DataType::CIDR, "", "direct"},
            {"POSTGRESQL", "MACADDR", EmulatedStorageKind::NATIVE, DataType::MACADDR, "", "direct"},
            {"POSTGRESQL", "MACADDR8", EmulatedStorageKind::NATIVE, DataType::MACADDR8, "", "direct"},
            {"POSTGRESQL", "JSON", EmulatedStorageKind::NATIVE, DataType::JSON, "", "direct"},
            {"POSTGRESQL", "JSONB", EmulatedStorageKind::NATIVE, DataType::JSONB, "", "direct"},
            {"POSTGRESQL", "XML", EmulatedStorageKind::NATIVE, DataType::XML, "", "direct"},
            {"POSTGRESQL", "TSVECTOR", EmulatedStorageKind::NATIVE, DataType::TSVECTOR, "", "pg format"},
            {"POSTGRESQL", "TSQUERY", EmulatedStorageKind::NATIVE, DataType::TSQUERY, "", "pg format"},
            {"POSTGRESQL", "PG_LSN", EmulatedStorageKind::DOMAIN, DataType::UINT64, "[sb_pg_dom]pg_lsn", "hex split format"},
            {"POSTGRESQL", "OID", EmulatedStorageKind::DOMAIN, DataType::UINT32, "[sb_pg_dom]oid", "oid domain"},
            {"POSTGRESQL", "XID", EmulatedStorageKind::DOMAIN, DataType::UINT32, "[sb_pg_dom]xid", "xid domain"},
            {"POSTGRESQL", "XID8", EmulatedStorageKind::DOMAIN, DataType::UINT64, "[sb_pg_dom]xid8", "xid8 domain"},
            {"POSTGRESQL", "POINT", EmulatedStorageKind::NATIVE, DataType::POINT, "", "direct"},
            {"POSTGRESQL", "LINE", EmulatedStorageKind::DOMAIN, DataType::GEOMETRY, "[sb_pg_dom]line", "geometry kind line"},
            {"POSTGRESQL", "LSEG", EmulatedStorageKind::DOMAIN, DataType::GEOMETRY, "[sb_pg_dom]lseg", "geometry kind lseg"},
            {"POSTGRESQL", "BOX", EmulatedStorageKind::DOMAIN, DataType::GEOMETRY, "[sb_pg_dom]box", "geometry kind box"},
            {"POSTGRESQL", "PATH", EmulatedStorageKind::DOMAIN, DataType::GEOMETRY, "[sb_pg_dom]path", "geometry kind path"},
            {"POSTGRESQL", "POLYGON", EmulatedStorageKind::NATIVE, DataType::POLYGON, "", "direct"},
            {"POSTGRESQL", "CIRCLE", EmulatedStorageKind::DOMAIN, DataType::GEOMETRY, "[sb_pg_dom]circle", "geometry kind circle"},
            {"POSTGRESQL", "INT4RANGE", EmulatedStorageKind::NATIVE, DataType::INT4RANGE, "", "range"},
            {"POSTGRESQL", "INT8RANGE", EmulatedStorageKind::NATIVE, DataType::INT8RANGE, "", "range"},
            {"POSTGRESQL", "NUMRANGE", EmulatedStorageKind::NATIVE, DataType::NUMRANGE, "", "range"},
            {"POSTGRESQL", "TSRANGE", EmulatedStorageKind::NATIVE, DataType::TSRANGE, "", "range"},
            {"POSTGRESQL", "TSTZRANGE", EmulatedStorageKind::NATIVE, DataType::TSTZRANGE, "", "range"},
            {"POSTGRESQL", "DATERANGE", EmulatedStorageKind::NATIVE, DataType::DATERANGE, "", "range"},
            {"POSTGRESQL", "MULTIRANGE", EmulatedStorageKind::DOMAIN, DataType::ARRAY, "[sb_pg_dom]multirange", "array of ranges"},
            {"POSTGRESQL", "ARRAY", EmulatedStorageKind::NATIVE, DataType::ARRAY, "", "pg array"},
            {"POSTGRESQL", "COMPOSITE", EmulatedStorageKind::NATIVE, DataType::COMPOSITE, "", "record"},
            {"POSTGRESQL", "ENUM", EmulatedStorageKind::NATIVE, DataType::ENUM, "", "ordinal labels"},

            // MySQL scalar/complex
            {"MYSQL", "TINYINT", EmulatedStorageKind::NATIVE, DataType::INT8, "", "signed/unsigned parser gate"},
            {"MYSQL", "SMALLINT", EmulatedStorageKind::NATIVE, DataType::INT16, "", "signed/unsigned parser gate"},
            {"MYSQL", "MEDIUMINT", EmulatedStorageKind::DOMAIN, DataType::INT32, "[sb_my_dom]mediumint_signed", "24-bit range"},
            {"MYSQL", "INT", EmulatedStorageKind::NATIVE, DataType::INT32, "", "signed/unsigned parser gate"},
            {"MYSQL", "INTEGER", EmulatedStorageKind::NATIVE, DataType::INT32, "", "signed/unsigned parser gate"},
            {"MYSQL", "BIGINT", EmulatedStorageKind::NATIVE, DataType::INT64, "", "signed/unsigned parser gate"},
            {"MYSQL", "DECIMAL", EmulatedStorageKind::DOMAIN, DataType::DECIMAL, "[sb_my_dom]decimal_unsigned", "numeric_mode parser gate"},
            {"MYSQL", "NUMERIC", EmulatedStorageKind::DOMAIN, DataType::DECIMAL, "[sb_my_dom]decimal_unsigned", "numeric_mode parser gate"},
            {"MYSQL", "FLOAT", EmulatedStorageKind::NATIVE, DataType::FLOAT32, "", "direct"},
            {"MYSQL", "DOUBLE", EmulatedStorageKind::NATIVE, DataType::FLOAT64, "", "direct"},
            {"MYSQL", "BIT", EmulatedStorageKind::DOMAIN, DataType::BIT, "[sb_my_dom]bit", "bit length"},
            {"MYSQL", "BOOL", EmulatedStorageKind::DOMAIN, DataType::INT8, "[sb_my_dom]bool", "0/1 bool"},
            {"MYSQL", "BOOLEAN", EmulatedStorageKind::DOMAIN, DataType::INT8, "[sb_my_dom]bool", "0/1 bool"},
            {"MYSQL", "DATE", EmulatedStorageKind::NATIVE, DataType::DATE, "", "direct"},
            {"MYSQL", "TIME", EmulatedStorageKind::DOMAIN, DataType::INTERVAL, "[sb_my_dom]time_interval", "duration semantics"},
            {"MYSQL", "DATETIME", EmulatedStorageKind::NATIVE, DataType::TIMESTAMP, "", "mysql datetime"},
            {"MYSQL", "TIMESTAMP", EmulatedStorageKind::NATIVE, DataType::TIMESTAMP_WITH_ZONE, "", "session timezone"},
            {"MYSQL", "YEAR", EmulatedStorageKind::DOMAIN, DataType::YEAR, "[sb_my_dom]year", "year constraints"},
            {"MYSQL", "CHAR", EmulatedStorageKind::NATIVE, DataType::CHAR, "", "length chars"},
            {"MYSQL", "VARCHAR", EmulatedStorageKind::NATIVE, DataType::VARCHAR, "", "length chars"},
            {"MYSQL", "BINARY", EmulatedStorageKind::NATIVE, DataType::BINARY, "", "length bytes"},
            {"MYSQL", "VARBINARY", EmulatedStorageKind::NATIVE, DataType::VARBINARY, "", "length bytes"},
            {"MYSQL", "TEXT", EmulatedStorageKind::DOMAIN, DataType::TEXT, "[sb_my_dom]text", "size class parser gate"},
            {"MYSQL", "BLOB", EmulatedStorageKind::DOMAIN, DataType::BLOB, "[sb_my_dom]blob", "size class parser gate"},
            {"MYSQL", "ENUM", EmulatedStorageKind::NATIVE, DataType::ENUM, "", "labels"},
            {"MYSQL", "SET", EmulatedStorageKind::NATIVE, DataType::SET, "", "labels"},
            {"MYSQL", "JSON", EmulatedStorageKind::NATIVE, DataType::JSON, "", "direct"},
            {"MYSQL", "GEOMETRY", EmulatedStorageKind::NATIVE, DataType::GEOMETRY, "", "wkb/ewkb"},
            {"MYSQL", "POINT", EmulatedStorageKind::NATIVE, DataType::POINT, "", "direct"},
            {"MYSQL", "LINESTRING", EmulatedStorageKind::NATIVE, DataType::LINESTRING, "", "direct"},
            {"MYSQL", "POLYGON", EmulatedStorageKind::NATIVE, DataType::POLYGON, "", "direct"},
            {"MYSQL", "MULTIPOINT", EmulatedStorageKind::NATIVE, DataType::MULTIPOINT, "", "direct"},
            {"MYSQL", "MULTILINESTRING", EmulatedStorageKind::NATIVE, DataType::MULTILINESTRING, "", "direct"},
            {"MYSQL", "MULTIPOLYGON", EmulatedStorageKind::NATIVE, DataType::MULTIPOLYGON, "", "direct"},
            {"MYSQL", "GEOMETRYCOLLECTION", EmulatedStorageKind::NATIVE, DataType::GEOMETRYCOLLECTION, "", "direct"},

            // Cassandra scalar/complex
            {"CASSANDRA", "ASCII", EmulatedStorageKind::DOMAIN, DataType::TEXT, "[sb_cas_dom]ascii", "ascii enforcement"},
            {"CASSANDRA", "TEXT", EmulatedStorageKind::NATIVE, DataType::TEXT, "", "utf8"},
            {"CASSANDRA", "VARCHAR", EmulatedStorageKind::NATIVE, DataType::TEXT, "", "utf8"},
            {"CASSANDRA", "TINYINT", EmulatedStorageKind::NATIVE, DataType::INT8, "", "direct"},
            {"CASSANDRA", "SMALLINT", EmulatedStorageKind::NATIVE, DataType::INT16, "", "direct"},
            {"CASSANDRA", "INT", EmulatedStorageKind::NATIVE, DataType::INT32, "", "direct"},
            {"CASSANDRA", "BIGINT", EmulatedStorageKind::NATIVE, DataType::INT64, "", "direct"},
            {"CASSANDRA", "VARINT", EmulatedStorageKind::DOMAIN, DataType::DECIMAL, "[sb_cas_dom]varint", "precision=0 scale=0"},
            {"CASSANDRA", "DECIMAL", EmulatedStorageKind::DOMAIN, DataType::DECIMAL, "[sb_cas_dom]decimal", "precision=0"},
            {"CASSANDRA", "FLOAT", EmulatedStorageKind::NATIVE, DataType::FLOAT32, "", "direct"},
            {"CASSANDRA", "DOUBLE", EmulatedStorageKind::NATIVE, DataType::FLOAT64, "", "direct"},
            {"CASSANDRA", "BOOLEAN", EmulatedStorageKind::NATIVE, DataType::BOOLEAN, "", "direct"},
            {"CASSANDRA", "DATE", EmulatedStorageKind::NATIVE, DataType::DATE, "", "cql epoch"},
            {"CASSANDRA", "TIME", EmulatedStorageKind::DOMAIN, DataType::INT64, "[sb_cas_dom]time", "nanoseconds"},
            {"CASSANDRA", "TIMESTAMP", EmulatedStorageKind::NATIVE, DataType::TIMESTAMP, "", "ms->us"},
            {"CASSANDRA", "DURATION", EmulatedStorageKind::DOMAIN, DataType::COMPOSITE, "[sb_cas_dom]duration", "months/days/nanos"},
            {"CASSANDRA", "UUID", EmulatedStorageKind::NATIVE, DataType::UUID, "", "direct"},
            {"CASSANDRA", "TIMEUUID", EmulatedStorageKind::DOMAIN, DataType::UUID, "[sb_cas_dom]timeuuid", "ordered uuid"},
            {"CASSANDRA", "INET", EmulatedStorageKind::NATIVE, DataType::INET, "", "direct"},
            {"CASSANDRA", "BLOB", EmulatedStorageKind::NATIVE, DataType::BLOB, "", "direct"},
            {"CASSANDRA", "COUNTER", EmulatedStorageKind::DOMAIN, DataType::INT64, "[sb_cas_dom]counter", "inc/dec only"},
            {"CASSANDRA", "LIST", EmulatedStorageKind::NATIVE, DataType::ARRAY, "", "ordered"},
            {"CASSANDRA", "SET", EmulatedStorageKind::NATIVE, DataType::SET, "", "unique sorted"},
            {"CASSANDRA", "MAP", EmulatedStorageKind::NATIVE, DataType::MAP, "", "map"},
            {"CASSANDRA", "TUPLE", EmulatedStorageKind::NATIVE, DataType::COMPOSITE, "", "ordered fields"},
            {"CASSANDRA", "UDT", EmulatedStorageKind::NATIVE, DataType::COMPOSITE, "", "named fields"},
            {"CASSANDRA", "FROZEN", EmulatedStorageKind::DOMAIN, DataType::COMPOSITE, "[sb_cas_dom]frozen", "update as one"},
            {"CASSANDRA", "VECTOR", EmulatedStorageKind::NATIVE, DataType::VECTOR, "", "vector with dim"},

            // Milvus scalar/complex
            {"MILVUS", "BOOL", EmulatedStorageKind::NATIVE, DataType::BOOLEAN, "", "direct"},
            {"MILVUS", "INT8", EmulatedStorageKind::NATIVE, DataType::INT8, "", "direct"},
            {"MILVUS", "INT16", EmulatedStorageKind::NATIVE, DataType::INT16, "", "direct"},
            {"MILVUS", "INT32", EmulatedStorageKind::NATIVE, DataType::INT32, "", "direct"},
            {"MILVUS", "INT64", EmulatedStorageKind::NATIVE, DataType::INT64, "", "direct"},
            {"MILVUS", "FLOAT", EmulatedStorageKind::NATIVE, DataType::FLOAT32, "", "direct"},
            {"MILVUS", "DOUBLE", EmulatedStorageKind::NATIVE, DataType::FLOAT64, "", "direct"},
            {"MILVUS", "VARCHAR", EmulatedStorageKind::DOMAIN, DataType::VARCHAR, "[sb_mil_dom]varchar", "length chars"},
            {"MILVUS", "TEXT", EmulatedStorageKind::NATIVE, DataType::TEXT, "", "direct"},
            {"MILVUS", "STRING", EmulatedStorageKind::DOMAIN, DataType::TEXT, "[sb_mil_dom]string", "alias"},
            {"MILVUS", "JSON", EmulatedStorageKind::NATIVE, DataType::JSON, "", "direct"},
            {"MILVUS", "GEOMETRY", EmulatedStorageKind::NATIVE, DataType::GEOMETRY, "", "wkb/ewkb"},
            {"MILVUS", "TIMESTAMPTZ", EmulatedStorageKind::NATIVE, DataType::TIMESTAMP_WITH_ZONE, "", "direct"},
            {"MILVUS", "BINARYVECTOR", EmulatedStorageKind::NATIVE, DataType::VECTOR, "", "binary vector"},
            {"MILVUS", "FLOATVECTOR", EmulatedStorageKind::NATIVE, DataType::VECTOR, "", "float32 vector"},
            {"MILVUS", "FLOAT16VECTOR", EmulatedStorageKind::NATIVE, DataType::VECTOR, "", "float16 vector"},
            {"MILVUS", "BFLOAT16VECTOR", EmulatedStorageKind::NATIVE, DataType::VECTOR, "", "bfloat16 vector"},
            {"MILVUS", "INT8VECTOR", EmulatedStorageKind::NATIVE, DataType::VECTOR, "", "int8 vector"},
            {"MILVUS", "SPARSEFLOATVECTOR", EmulatedStorageKind::NATIVE, DataType::VECTOR, "", "sparse vector"},
            {"MILVUS", "ARRAY", EmulatedStorageKind::NATIVE, DataType::ARRAY, "", "element type uuid"},
            {"MILVUS", "ARRAYOFVECTOR", EmulatedStorageKind::DOMAIN, DataType::ARRAY, "[sb_mil_dom]array_of_vector", "array<vector>"},
            {"MILVUS", "ARRAYOFSTRUCT", EmulatedStorageKind::DOMAIN, DataType::ARRAY, "[sb_mil_dom]array_of_struct", "array<composite>"},

            // MongoDB scalar/complex
            {"MONGODB", "DOUBLE", EmulatedStorageKind::NATIVE, DataType::FLOAT64, "", "direct"},
            {"MONGODB", "STRING", EmulatedStorageKind::NATIVE, DataType::TEXT, "", "utf8"},
            {"MONGODB", "OBJECTID", EmulatedStorageKind::DOMAIN, DataType::BLOB, "[sb_mongo_dom]objectid", "12-byte oid"},
            {"MONGODB", "BOOLEAN", EmulatedStorageKind::NATIVE, DataType::BOOLEAN, "", "direct"},
            {"MONGODB", "DATE", EmulatedStorageKind::NATIVE, DataType::TIMESTAMP, "", "ms epoch"},
            {"MONGODB", "NULL", EmulatedStorageKind::DOMAIN, DataType::NULL_TYPE, "[sb_mongo_dom]null", "null sentinel"},
            {"MONGODB", "INT32", EmulatedStorageKind::NATIVE, DataType::INT32, "", "direct"},
            {"MONGODB", "INT64", EmulatedStorageKind::NATIVE, DataType::INT64, "", "direct"},
            {"MONGODB", "DECIMAL128", EmulatedStorageKind::NATIVE, DataType::DECFLOAT34, "", "direct"},
            {"MONGODB", "TIMESTAMP", EmulatedStorageKind::DOMAIN, DataType::UINT64, "[sb_mongo_dom]timestamp", "ts u64"},
            {"MONGODB", "BINARY", EmulatedStorageKind::DOMAIN, DataType::BLOB, "[sb_mongo_dom]bin_data", "subtype preserved"},
            {"MONGODB", "REGEX", EmulatedStorageKind::DOMAIN, DataType::TEXT, "[sb_mongo_dom]regex", "pattern/flags"},
            {"MONGODB", "JAVASCRIPT", EmulatedStorageKind::DOMAIN, DataType::TEXT, "[sb_mongo_dom]javascript", "code"},
            {"MONGODB", "JAVASCRIPTWITHSCOPE", EmulatedStorageKind::DOMAIN, DataType::COMPOSITE, "[sb_mongo_dom]javascript_scope", "code+scope"},
            {"MONGODB", "MINKEY", EmulatedStorageKind::DOMAIN, DataType::UINT8, "[sb_mongo_dom]minkey", "sentinel"},
            {"MONGODB", "MAXKEY", EmulatedStorageKind::DOMAIN, DataType::UINT8, "[sb_mongo_dom]maxkey", "sentinel"},
            {"MONGODB", "UNDEFINED", EmulatedStorageKind::DOMAIN, DataType::UINT8, "[sb_mongo_dom]undefined", "deprecated"},
            {"MONGODB", "DBPOINTER", EmulatedStorageKind::DOMAIN, DataType::COMPOSITE, "[sb_mongo_dom]dbpointer", "namespace+oid"},
            {"MONGODB", "SYMBOL", EmulatedStorageKind::DOMAIN, DataType::TEXT, "[sb_mongo_dom]symbol", "deprecated"},
            {"MONGODB", "DOCUMENT", EmulatedStorageKind::NATIVE, DataType::BSON, "", "bson document"},
            {"MONGODB", "ARRAY", EmulatedStorageKind::DOMAIN, DataType::BSON, "[sb_mongo_dom]array", "bson array"},

            // Neo4j scalar/complex
            {"NEO4J", "BOOLEAN", EmulatedStorageKind::NATIVE, DataType::BOOLEAN, "", "direct"},
            {"NEO4J", "INTEGER", EmulatedStorageKind::NATIVE, DataType::INT64, "", "64-bit integer"},
            {"NEO4J", "FLOAT", EmulatedStorageKind::NATIVE, DataType::FLOAT64, "", "direct"},
            {"NEO4J", "STRING", EmulatedStorageKind::NATIVE, DataType::TEXT, "", "utf8"},
            {"NEO4J", "BYTEARRAY", EmulatedStorageKind::NATIVE, DataType::BLOB, "", "direct"},
            {"NEO4J", "DATE", EmulatedStorageKind::NATIVE, DataType::DATE, "", "direct"},
            {"NEO4J", "TIME", EmulatedStorageKind::NATIVE, DataType::TIME_WITH_ZONE, "", "offset preserved"},
            {"NEO4J", "LOCALTIME", EmulatedStorageKind::NATIVE, DataType::TIME, "", "no zone"},
            {"NEO4J", "DATETIME", EmulatedStorageKind::NATIVE, DataType::TIMESTAMP_WITH_ZONE, "", "offset preserved"},
            {"NEO4J", "LOCALDATETIME", EmulatedStorageKind::NATIVE, DataType::TIMESTAMP, "", "no zone"},
            {"NEO4J", "DURATION", EmulatedStorageKind::NATIVE, DataType::INTERVAL, "", "months/days/micros"},
            {"NEO4J", "LIST", EmulatedStorageKind::NATIVE, DataType::ARRAY, "", "ordered"},
            {"NEO4J", "POINT", EmulatedStorageKind::NATIVE, DataType::POINT, "", "crs/srid"},

            // Redis scalar/complex
            {"REDIS", "STRING", EmulatedStorageKind::NATIVE, DataType::BLOB, "", "binary-safe"},
            {"REDIS", "LIST", EmulatedStorageKind::DOMAIN, DataType::LIST, "[sb_redis_dom]list", "ordered list"},
            {"REDIS", "SET", EmulatedStorageKind::DOMAIN, DataType::SET, "[sb_redis_dom]set", "unique set"},
            {"REDIS", "ZSET", EmulatedStorageKind::DOMAIN, DataType::LIST, "[sb_redis_dom]zset", "score/value composite"},
            {"REDIS", "HASH", EmulatedStorageKind::DOMAIN, DataType::MAP, "[sb_redis_dom]hash", "field/value map"},
            {"REDIS", "STREAM", EmulatedStorageKind::DOMAIN, DataType::LIST, "[sb_redis_dom]stream", "ordered entries"},
            {"REDIS", "GEO", EmulatedStorageKind::DOMAIN, DataType::LIST, "[sb_redis_dom]geo", "geohash member payload"},
            {"REDIS", "HLL", EmulatedStorageKind::DOMAIN, DataType::MAP, "[sb_redis_dom]hll", "register map"},
            {"REDIS", "BITMAP", EmulatedStorageKind::DOMAIN, DataType::LIST, "[sb_redis_dom]bitmap", "byte-addressable bitmap"},

            // ClickHouse vNext coverage
            {"CLICKHOUSE", "INT256", EmulatedStorageKind::NATIVE, DataType::INT256, "", "direct"},
            {"CLICKHOUSE", "UINT256", EmulatedStorageKind::NATIVE, DataType::UINT256, "", "direct"},
            {"CLICKHOUSE", "DECIMAL256", EmulatedStorageKind::NATIVE, DataType::DECIMAL256, "", "precision/scale"},
            {"CLICKHOUSE", "LOWCARDINALITY", EmulatedStorageKind::DOMAIN, DataType::DICT_ENCODED, "[sb_dom]dict_encoded", "dictionary encoded"},

            // Influx/OpenSearch/DuckDB vNext coverage
            {"INFLUXDB", "TIMESTAMP", EmulatedStorageKind::NATIVE, DataType::TIMESTAMP_NS, "", "nanosecond epoch"},
            {"DUCKDB", "UNION", EmulatedStorageKind::NATIVE, DataType::TAGGED_UNION, "", "tagged union"},
            {"DUCKDB", "TIMESTAMP_NS", EmulatedStorageKind::NATIVE, DataType::TIMESTAMP_NS, "", "direct"},
            {"OPENSEARCH", "COMPLETION", EmulatedStorageKind::DOMAIN, DataType::COMPLETION_FIELD, "[sb_dom]completion_field", "completion field"},
            {"OPENSEARCH", "SEARCH_AS_YOU_TYPE", EmulatedStorageKind::DOMAIN, DataType::PREFIX_SEARCH_FIELD, "[sb_dom]prefix_search_field", "prefix field"},
            {"OPENSEARCH", "FLAT_OBJECT", EmulatedStorageKind::DOMAIN, DataType::FLAT_OBJECT, "[sb_dom]flat_object", "flattened field"},
        };

        bool isStringType(DataType type)
        {
            return type == DataType::CHAR || type == DataType::VARCHAR || type == DataType::TEXT ||
                   type == DataType::COMPLETION_FIELD || type == DataType::PREFIX_SEARCH_FIELD;
        }

        bool isIntegerType(DataType type)
        {
            return type == DataType::INT8 || type == DataType::INT16 || type == DataType::INT32 ||
                   type == DataType::INT64 || type == DataType::INT128 || type == DataType::UINT8 ||
                   type == DataType::UINT16 || type == DataType::UINT32 ||
                   type == DataType::UINT64 || type == DataType::UINT128 ||
                   type == DataType::INT256 || type == DataType::UINT256;
        }

        bool isFloatType(DataType type)
        {
            return type == DataType::FLOAT32 || type == DataType::FLOAT64;
        }

        bool isDecimalFamily(DataType type)
        {
            return type == DataType::DECIMAL || type == DataType::MONEY ||
                   type == DataType::DECFLOAT16 || type == DataType::DECFLOAT34 ||
                   type == DataType::DECIMAL256;
        }

        bool isNumericType(DataType type)
        {
            return isIntegerType(type) || isFloatType(type) || isDecimalFamily(type);
        }

        bool isTemporalType(DataType type)
        {
            return type == DataType::DATE || type == DataType::TIME ||
                   type == DataType::TIMESTAMP || type == DataType::TIMESTAMP_WITH_ZONE ||
                   type == DataType::TIME_WITH_ZONE || type == DataType::INTERVAL ||
                   type == DataType::TIMESTAMP_NS;
        }

        bool isBinaryType(DataType type)
        {
            return type == DataType::BINARY || type == DataType::VARBINARY ||
                   type == DataType::BLOB || type == DataType::BYTEA || type == DataType::VECTOR;
        }

        bool isJsonFamily(DataType type)
        {
            return type == DataType::JSON || type == DataType::JSONB ||
                   type == DataType::BSON || type == DataType::XML;
        }

        bool isNetworkType(DataType type)
        {
            return type == DataType::INET || type == DataType::CIDR ||
                   type == DataType::MACADDR || type == DataType::MACADDR8;
        }

        bool isContainerType(DataType type)
        {
            return type == DataType::ARRAY || type == DataType::LIST ||
                   type == DataType::COMPOSITE || type == DataType::MAP ||
                   type == DataType::VARIANT || type == DataType::SET ||
                   type == DataType::ENUM || type == DataType::ROW ||
                   type == DataType::TAGGED_UNION || type == DataType::DICT_ENCODED ||
                   type == DataType::FLAT_OBJECT;
        }

        bool isGeometryType(DataType type)
        {
            return type == DataType::GEOMETRY || type == DataType::POINT ||
                   type == DataType::LINESTRING || type == DataType::POLYGON ||
                   type == DataType::MULTIPOINT || type == DataType::MULTILINESTRING ||
                   type == DataType::MULTIPOLYGON || type == DataType::GEOMETRYCOLLECTION;
        }

        bool parserRuleRequiresWholeValueUpdate(const char* parser_rule_hint)
        {
            if (!parser_rule_hint)
            {
                return false;
            }
            const std::string hint = normalizeSpaces(uppercaseAscii(parser_rule_hint));
            return hint.find("UPDATE AS ONE") != std::string::npos ||
                   hint.find("WHOLE VALUE") != std::string::npos;
        }
    } // namespace

    const char* TypeSystem::getTypeName(DataType type)
    {
        switch (type)
        {
            case DataType::UNKNOWN: return "UNKNOWN";
            case DataType::INT8: return "INT8";
            case DataType::INT16: return "INT16";
            case DataType::INT32: return "INT32";
            case DataType::INT64: return "INT64";
            case DataType::INT128: return "INT128";
            case DataType::INT256: return "INT256";
            case DataType::UINT8: return "UINT8";
            case DataType::UINT16: return "UINT16";
            case DataType::UINT32: return "UINT32";
            case DataType::UINT64: return "UINT64";
            case DataType::UINT128: return "UINT128";
            case DataType::UINT256: return "UINT256";
            case DataType::FLOAT32: return "FLOAT32";
            case DataType::FLOAT64: return "FLOAT64";
            case DataType::DECIMAL: return "DECIMAL";
            case DataType::DECIMAL256: return "DECIMAL256";
            case DataType::MONEY: return "MONEY";
            case DataType::DECFLOAT16: return "DECFLOAT(16)";
            case DataType::DECFLOAT34: return "DECFLOAT(34)";
            case DataType::CHAR: return "CHAR";
            case DataType::VARCHAR: return "VARCHAR";
            case DataType::TEXT: return "TEXT";
            case DataType::BINARY: return "BINARY";
            case DataType::VARBINARY: return "VARBINARY";
            case DataType::BLOB: return "BLOB";
            case DataType::BYTEA: return "BYTEA";
            case DataType::DATE: return "DATE";
            case DataType::TIME: return "TIME";
            case DataType::TIMESTAMP: return "TIMESTAMP";
            case DataType::TIMESTAMP_NS: return "TIMESTAMP_NS";
            case DataType::TIMESTAMP_WITH_ZONE: return "TIMESTAMP_WITH_ZONE";
            case DataType::TIME_WITH_ZONE: return "TIME_WITH_ZONE";
            case DataType::INTERVAL: return "INTERVAL";
            case DataType::YEAR: return "YEAR";
            case DataType::BOOLEAN: return "BOOLEAN";
            case DataType::UUID: return "UUID";
            case DataType::JSON: return "JSON";
            case DataType::JSONB: return "JSONB";
            case DataType::XML: return "XML";
            case DataType::VECTOR: return "VECTOR";
            case DataType::POINT: return "POINT";
            case DataType::LINESTRING: return "LINESTRING";
            case DataType::POLYGON: return "POLYGON";
            case DataType::MULTIPOINT: return "MULTIPOINT";
            case DataType::MULTILINESTRING: return "MULTILINESTRING";
            case DataType::MULTIPOLYGON: return "MULTIPOLYGON";
            case DataType::GEOMETRYCOLLECTION: return "GEOMETRYCOLLECTION";
            case DataType::ARRAY: return "ARRAY";
            case DataType::COMPOSITE: return "COMPOSITE";
            case DataType::LIST: return "LIST";
            case DataType::MAP: return "MAP";
            case DataType::BSON: return "BSON";
            case DataType::TSVECTOR: return "TSVECTOR";
            case DataType::TSQUERY: return "TSQUERY";
            case DataType::INT4RANGE: return "INT4RANGE";
            case DataType::INT8RANGE: return "INT8RANGE";
            case DataType::NUMRANGE: return "NUMRANGE";
            case DataType::TSRANGE: return "TSRANGE";
            case DataType::TSTZRANGE: return "TSTZRANGE";
            case DataType::DATERANGE: return "DATERANGE";
            case DataType::INET: return "INET";
            case DataType::CIDR: return "CIDR";
            case DataType::MACADDR: return "MACADDR";
            case DataType::MACADDR8: return "MACADDR8";
            case DataType::DOMAIN: return "DOMAIN";
            case DataType::ROW: return "ROW";
            case DataType::ENUM: return "ENUM";
            case DataType::SET: return "SET";
            case DataType::VARIANT: return "VARIANT";
            case DataType::TAGGED_UNION: return "TAGGED_UNION";
            case DataType::DICT_ENCODED: return "DICT_ENCODED";
            case DataType::COMPLETION_FIELD: return "COMPLETION_FIELD";
            case DataType::PREFIX_SEARCH_FIELD: return "PREFIX_SEARCH_FIELD";
            case DataType::FLAT_OBJECT: return "FLAT_OBJECT";
            case DataType::BLOB_SUB_TYPE_TEXT: return "BLOB_SUB_TYPE_TEXT";
            case DataType::NULL_TYPE: return "NULL";
            default: return "UNKNOWN";
        }
    }

    bool TypeSystem::isExplicitlyConvertible(DataType from, DataType to)
    {
        // Same type is always convertible
        if (from == to) return true;

        // NULL can convert to any type
        if (from == DataType::NULL_TYPE) return true;

        auto is_vnext_type = [](DataType type) -> bool
        {
            return type == DataType::TIMESTAMP_NS ||
                   type == DataType::INT256 ||
                   type == DataType::UINT256 ||
                   type == DataType::DECIMAL256 ||
                   type == DataType::TAGGED_UNION ||
                   type == DataType::DICT_ENCODED ||
                   type == DataType::COMPLETION_FIELD ||
                   type == DataType::PREFIX_SEARCH_FIELD ||
                   type == DataType::FLAT_OBJECT;
        };
        auto is_scalar_type = [&](DataType type) -> bool
        {
            if (type == DataType::UNKNOWN || type == DataType::NULL_TYPE)
            {
                return false;
            }
            if (isContainerType(type) || isGeometryType(type))
            {
                return false;
            }
            return true;
        };

        if (is_vnext_type(from) || is_vnext_type(to))
        {
            if ((from == DataType::INT256 && to == DataType::DECIMAL256) ||
                (from == DataType::UINT256 && to == DataType::DECIMAL256) ||
                (from == DataType::DECIMAL256 && to == DataType::INT256) ||
                (from == DataType::DECIMAL256 && to == DataType::UINT256) ||
                (from == DataType::UINT256 && to == DataType::INT256))
            {
                return true;
            }
            if ((from == DataType::TIMESTAMP_NS && to == DataType::TIMESTAMP) ||
                (from == DataType::TIMESTAMP_NS && to == DataType::TIMESTAMP_WITH_ZONE) ||
                (to == DataType::TIMESTAMP_NS && from == DataType::TIMESTAMP) ||
                (to == DataType::TIMESTAMP_NS && from == DataType::TIMESTAMP_WITH_ZONE))
            {
                return true;
            }
            if ((from == DataType::TAGGED_UNION && is_scalar_type(to)) ||
                (to == DataType::TAGGED_UNION && is_scalar_type(from)))
            {
                return true;
            }
            if ((from == DataType::DICT_ENCODED && is_scalar_type(to)) ||
                (to == DataType::DICT_ENCODED && is_scalar_type(from)))
            {
                return true;
            }
            if (to == DataType::COMPLETION_FIELD && is_scalar_type(from))
            {
                return true;
            }
            if (from == DataType::FLAT_OBJECT && isJsonFamily(to))
            {
                return true;
            }
            if (from == DataType::PREFIX_SEARCH_FIELD && isStringType(to))
            {
                return true;
            }
            return false;
        }

        // Numeric matrix
        if (isNumericType(from) && isNumericType(to))
        {
            return true;
        }

        // BOOLEAN explicit cast matrix
        if ((from == DataType::BOOLEAN && (to == DataType::INT8 || isNumericType(to))) ||
            (to == DataType::BOOLEAN && (from == DataType::INT8 || isNumericType(from))))
        {
            return true;
        }

        // TEXT/BINARY matrix
        if (isStringType(from) && isStringType(to))
        {
            return true;
        }
        if (isBinaryType(from) && isBinaryType(to))
        {
            return true;
        }
        if ((isStringType(from) && isBinaryType(to)) || (isBinaryType(from) && isStringType(to)))
        {
            return true;
        }

        // Temporal matrix
        if ((from == DataType::DATE && to == DataType::TIMESTAMP) ||
            (from == DataType::TIMESTAMP && to == DataType::DATE) ||
            (from == DataType::TIME && to == DataType::TIMESTAMP) ||
            (from == DataType::TIMESTAMP && to == DataType::TIME) ||
            (from == DataType::TIMESTAMP && to == DataType::TIMESTAMP_WITH_ZONE) ||
            (from == DataType::TIMESTAMP_WITH_ZONE && to == DataType::TIMESTAMP) ||
            (from == DataType::TIMESTAMP_NS && to == DataType::TIMESTAMP) ||
            (from == DataType::TIMESTAMP && to == DataType::TIMESTAMP_NS) ||
            (from == DataType::TIMESTAMP_NS && to == DataType::TIMESTAMP_WITH_ZONE) ||
            (from == DataType::TIMESTAMP_WITH_ZONE && to == DataType::TIMESTAMP_NS) ||
            (from == DataType::TIME && to == DataType::TIME_WITH_ZONE) ||
            (from == DataType::TIME_WITH_ZONE && to == DataType::TIME))
        {
            return true;
        }

        // UUID matrix
        if ((from == DataType::UUID && isStringType(to)) ||
            (to == DataType::UUID && isStringType(from)) ||
            (from == DataType::UUID && isBinaryType(to)) ||
            (to == DataType::UUID && isBinaryType(from)))
        {
            return true;
        }

        // Network matrix
        if ((from == DataType::INET && to == DataType::CIDR) ||
            (from == DataType::CIDR && to == DataType::INET))
        {
            return true;
        }
        if ((isNetworkType(from) && isStringType(to)) || (isStringType(from) && isNetworkType(to)))
        {
            return true;
        }

        // JSON/JSONB/BSON/XML matrix
        if (isJsonFamily(from) && isJsonFamily(to))
        {
            return true;
        }
        if ((isStringType(from) && isJsonFamily(to)) || (isJsonFamily(from) && isStringType(to)))
        {
            return true;
        }
        if ((isContainerType(from) && isJsonFamily(to)) ||
            (isJsonFamily(from) && isContainerType(to)))
        {
            return true;
        }

        // Geometry matrix
        if ((isStringType(from) && isGeometryType(to)) ||
            (isGeometryType(from) && isStringType(to)))
        {
            return true;
        }

        // Container/domain-render matrix
        if ((isContainerType(from) && isStringType(to)) ||
            (isStringType(from) && isContainerType(to)))
        {
            return true;
        }

        // Scalar text parsing matrix
        if ((isStringType(from) && (isNumericType(to) || to == DataType::BOOLEAN ||
                                    isTemporalType(to) || to == DataType::UUID)) ||
            ((isNumericType(from) || from == DataType::BOOLEAN || isTemporalType(from)) &&
             isStringType(to)))
        {
            return true;
        }

        return false;
    }

    bool TypeSystem::resolveEmulatedType(const std::string& engine_name,
                                         const std::string& emulated_type,
                                         EmulatedTypeMapping& out)
    {
        const std::string normalized_engine = normalizeEngineName(engine_name);
        const std::string normalized_type = normalizeEmulatedTypeName(emulated_type);
        if (normalized_engine.empty() || normalized_type.empty())
        {
            return false;
        }

        for (const auto& row : kEmulationTypeMatrix)
        {
            if (normalized_engine == row.engine_name && normalized_type == row.emulated_type)
            {
                out.engine_name = row.engine_name;
                out.emulated_type = row.emulated_type;
                out.storage_kind = row.storage_kind;
                out.canonical_type = row.canonical_type;
                out.domain_hint = row.domain_hint;
                out.parser_rule_hint = row.parser_rule_hint;
                return true;
            }
        }
        return false;
    }

    bool TypeSystem::requiresWholeValueUpdate(const std::string& engine_name,
                                              const std::string& emulated_type)
    {
        EmulatedTypeMapping mapping{};
        if (!resolveEmulatedType(engine_name, emulated_type, mapping))
        {
            return false;
        }
        return parserRuleRequiresWholeValueUpdate(mapping.parser_rule_hint);
    }

    bool TypeSystem::allowsElementLevelMutation(const std::string& engine_name,
                                                const std::string& emulated_type)
    {
        return !requiresWholeValueUpdate(engine_name, emulated_type);
    }

    bool TypeSystem::isToastEligibleType(DataType type)
    {
        if (isStringType(type) || isBinaryType(type) || isJsonFamily(type) ||
            isGeometryType(type) || type == DataType::TSVECTOR || type == DataType::TSQUERY)
        {
            return true;
        }

        return type == DataType::ARRAY || type == DataType::LIST ||
               type == DataType::MAP || type == DataType::COMPOSITE ||
               type == DataType::ROW || type == DataType::VARIANT ||
               type == DataType::TAGGED_UNION || type == DataType::DICT_ENCODED ||
               type == DataType::COMPLETION_FIELD ||
               type == DataType::PREFIX_SEARCH_FIELD ||
               type == DataType::FLAT_OBJECT;
    }

} // namespace scratchbird::core
