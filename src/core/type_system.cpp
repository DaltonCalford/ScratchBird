#include "scratchbird/core/types.h"

namespace scratchbird::core
{
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
            case DataType::UINT8: return "UINT8";
            case DataType::UINT16: return "UINT16";
            case DataType::UINT32: return "UINT32";
            case DataType::UINT64: return "UINT64";
            case DataType::UINT128: return "UINT128";
            case DataType::FLOAT32: return "FLOAT32";
            case DataType::FLOAT64: return "FLOAT64";
            case DataType::DECIMAL: return "DECIMAL";
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
            case DataType::INTERVAL: return "INTERVAL";
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
            case DataType::VARIANT: return "VARIANT";
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

        // Numeric types are generally convertible to each other
        bool from_numeric = (from >= DataType::INT8 && from <= DataType::UINT128) ||
                            from == DataType::DECFLOAT16 || from == DataType::DECFLOAT34;
        bool to_numeric = (to >= DataType::INT8 && to <= DataType::UINT128) ||
                          to == DataType::DECFLOAT16 || to == DataType::DECFLOAT34;
        if (from_numeric && to_numeric) return true;

        // String types are convertible to each other
        bool from_string = (from >= DataType::CHAR && from <= DataType::TEXT);
        bool to_string = (to >= DataType::CHAR && to <= DataType::TEXT);
        if (from_string && to_string) return true;

        // Binary types are convertible to each other
        bool from_binary = (from >= DataType::BINARY && from <= DataType::BYTEA);
        bool to_binary = (to >= DataType::BINARY && to <= DataType::BYTEA);
        if (from_binary && to_binary) return true;

        // Temporal types have some convertibility
        if (from == DataType::DATE && to == DataType::TIMESTAMP) return true;
        if (from == DataType::TIMESTAMP && to == DataType::DATE) return true;
        if (from == DataType::TIME && to == DataType::TIMESTAMP) return true;

        // JSON/JSONB are convertible to each other
        if ((from == DataType::JSON && to == DataType::JSONB) ||
            (from == DataType::JSONB && to == DataType::JSON)) return true;

        // Strings can be converted to most types (parsing)
        if (from_string) return true;

        // Most types can be converted to strings (formatting)
        if (to_string) return true;

        // Default: not convertible
        return false;
    }

} // namespace scratchbird::core
