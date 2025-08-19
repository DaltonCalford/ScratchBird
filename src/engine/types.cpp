#include "scratchbird/engine/types.h"

#include <algorithm>
#include <cctype>

namespace scratchbird
{
    namespace engine
    {

        static std::string lowercase(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c) { return char(std::tolower(c)); });
            return s;
        }

        static void trim(std::string& s)
        {
            auto not_space = [](int ch) { return !std::isspace(ch); };
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
            s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
        }

        TypeDescriptor parse_type_spec(const std::string& sql_type)
        {
            TypeDescriptor td{};
            if (sql_type.empty())
                return td;

            std::string s = lowercase(sql_type);
            trim(s);

            auto parse_param = [&](const char* key, int& out) {
                auto pos = s.find(key);
                if (pos == std::string::npos)
                    return false;
                auto lp = s.find('(', pos);
                auto rp = s.find(')', lp);
                if (lp == std::string::npos || rp == std::string::npos || rp <= lp + 1)
                    return false;
                out = std::stoi(s.substr(lp + 1, rp - lp - 1));
                return true;
            };

            if (s == "tinyint")
                td.kind = TypeKind::TinyInt;
            else if (s == "smallint")
                td.kind = TypeKind::SmallInt;
            else if (s == "integer" || s == "int")
                td.kind = TypeKind::Integer;
            else if (s == "bigint")
                td.kind = TypeKind::BigInt;
            else if (s.rfind("numeric", 0) == 0 || s.rfind("decimal", 0) == 0) {
                td.kind = (s.rfind("numeric", 0) == 0) ? TypeKind::Numeric : TypeKind::Decimal;
                auto lp = s.find('(');
                auto rp = s.find(')');
                if (lp != std::string::npos && rp != std::string::npos && rp > lp + 1) {
                    auto inner = s.substr(lp + 1, rp - lp - 1);
                    auto comma = inner.find(',');
                    if (comma == std::string::npos) {
                        td.precision = std::stoi(inner);
                    } else {
                        td.precision = std::stoi(inner.substr(0, comma));
                        td.scale = std::stoi(inner.substr(comma + 1));
                    }
                }
            } else if (s.rfind("float", 0) == 0) {
                td.kind = TypeKind::Float;
                int p = -1;
                if (parse_param("float", p))
                    td.precision = p;
            } else if (s == "double precision")
                td.kind = TypeKind::DoublePrecision;
            else if (s.rfind("char(", 0) == 0 || s.rfind("character(", 0) == 0) {
                td.kind = TypeKind::Char;
                int n = -1;
                if (parse_param("char", n) || parse_param("character", n))
                    td.length = n;
            } else if (s.rfind("varchar(", 0) == 0 || s.rfind("character varying(", 0) == 0) {
                td.kind = TypeKind::VarChar;
                int n = -1;
                if (parse_param("varchar", n) || parse_param("character varying", n))
                    td.length = n;
            } else if (s == "boolean")
                td.kind = TypeKind::Boolean;
            else if (s == "date")
                td.kind = TypeKind::Date;
            else if (s.rfind("time", 0) == 0 && s.find("zone") == std::string::npos)
                td.kind = TypeKind::Time;
            else if (s.find("time with time zone") != std::string::npos)
                td.kind = TypeKind::TimeTz;
            else if (s == "timestamp")
                td.kind = TypeKind::Timestamp;
            else if (s.find("timestamp with time zone") != std::string::npos)
                td.kind = TypeKind::TimestampTz;
            else if (s == "uuid")
                td.kind = TypeKind::Uuid;
            else if (s == "json")
                td.kind = TypeKind::Json;
            else if (s.rfind("blob", 0) == 0)
                td.kind = TypeKind::Blob;
            else if (s == "citext")
                td.kind = TypeKind::CiText;
            else if (s == "inet")
                td.kind = TypeKind::Inet;
            else if (s == "cidr")
                td.kind = TypeKind::Cidr;
            else if (s == "macaddr")
                td.kind = TypeKind::MacAddr;
            else if (s == "point")
                td.kind = TypeKind::Point;
            else if (s == "tsvector")
                td.kind = TypeKind::TsVector;
            else if (s == "tsquery")
                td.kind = TypeKind::TsQuery;
            else if (s.rfind("vector(", 0) == 0) {
                td.kind = TypeKind::Vector;
                auto lp = s.find('(');
                auto rp = s.find(')');
                if (lp != std::string::npos && rp != std::string::npos && rp > lp + 1)
                    td.vector_dims = std::stoi(s.substr(lp + 1, rp - lp - 1));
            } else if (s == "utinyint")
                td.kind = TypeKind::UTinyInt;
            else if (s == "usmallint")
                td.kind = TypeKind::USmallInt;
            else if (s == "uinteger")
                td.kind = TypeKind::UInteger;
            else if (s == "ubigint")
                td.kind = TypeKind::UBigInt;
            else if (s == "uint128")
                td.kind = TypeKind::UInt128;
            else if (s == "int4range")
                td.kind = TypeKind::Int4Range;
            else if (s == "int8range")
                td.kind = TypeKind::Int8Range;
            else if (s == "numrange")
                td.kind = TypeKind::NumRange;
            else if (s == "daterange")
                td.kind = TypeKind::DateRange;
            else if (s == "tsrange")
                td.kind = TypeKind::TsRange;
            else if (s == "tstzrange")
                td.kind = TypeKind::TstzRange;

            return td;
        }

        std::string to_string(const TypeDescriptor& td)
        {
            switch (td.kind) {
            case TypeKind::TinyInt:
                return "TINYINT";
            case TypeKind::SmallInt:
                return "SMALLINT";
            case TypeKind::Integer:
                return "INTEGER";
            case TypeKind::BigInt:
                return "BIGINT";
            case TypeKind::Int128:
                return "INT128";
            case TypeKind::UTinyInt:
                return "UTINYINT";
            case TypeKind::USmallInt:
                return "USMALLINT";
            case TypeKind::UInteger:
                return "UINTEGER";
            case TypeKind::UBigInt:
                return "UBIGINT";
            case TypeKind::UInt128:
                return "UINT128";
            case TypeKind::Numeric: {
                if (td.precision > 0 && td.scale >= 0)
                    return "NUMERIC(" + std::to_string(td.precision) + "," +
                           std::to_string(td.scale) + ")";
                if (td.precision > 0)
                    return "NUMERIC(" + std::to_string(td.precision) + ")";
                return "NUMERIC";
            }
            case TypeKind::Decimal: {
                if (td.precision > 0 && td.scale >= 0)
                    return "DECIMAL(" + std::to_string(td.precision) + "," +
                           std::to_string(td.scale) + ")";
                if (td.precision > 0)
                    return "DECIMAL(" + std::to_string(td.precision) + ")";
                return "DECIMAL";
            }
            case TypeKind::Float:
                return td.precision > 0 ? ("FLOAT(" + std::to_string(td.precision) + ")") : "FLOAT";
            case TypeKind::DoublePrecision:
                return "DOUBLE PRECISION";
            case TypeKind::DecFloat:
                return td.precision > 0 ? ("DECFLOAT(" + std::to_string(td.precision) + ")")
                                        : "DECFLOAT";
            case TypeKind::Char:
                return td.length > 0 ? ("CHAR(" + std::to_string(td.length) + ")") : "CHAR";
            case TypeKind::VarChar:
                return td.length > 0 ? ("VARCHAR(" + std::to_string(td.length) + ")") : "VARCHAR";
            case TypeKind::CiText:
                return "CITEXT";
            case TypeKind::Blob:
                return "BLOB";
            case TypeKind::Boolean:
                return "BOOLEAN";
            case TypeKind::Uuid:
                return "UUID";
            case TypeKind::Json:
                return "JSON";
            case TypeKind::Date:
                return "DATE";
            case TypeKind::Time:
                return "TIME";
            case TypeKind::TimeTz:
                return "TIME WITH TIME ZONE";
            case TypeKind::Timestamp:
                return "TIMESTAMP";
            case TypeKind::TimestampTz:
                return "TIMESTAMP WITH TIME ZONE";
            case TypeKind::Inet:
                return "INET";
            case TypeKind::Cidr:
                return "CIDR";
            case TypeKind::MacAddr:
                return "MACADDR";
            case TypeKind::Int4Range:
                return "INT4RANGE";
            case TypeKind::Int8Range:
                return "INT8RANGE";
            case TypeKind::NumRange:
                return "NUMRANGE";
            case TypeKind::DateRange:
                return "DATERANGE";
            case TypeKind::TsRange:
                return "TSRANGE";
            case TypeKind::TstzRange:
                return "TSTZRANGE";
            case TypeKind::Point:
                return "POINT";
            case TypeKind::TsVector:
                return "TSVECTOR";
            case TypeKind::TsQuery:
                return "TSQUERY";
            case TypeKind::Vector:
                return td.vector_dims > 0 ? ("VECTOR(" + std::to_string(td.vector_dims) + ")")
                                          : "VECTOR";
            case TypeKind::Composite:
                return "COMPOSITE";
            case TypeKind::Array:
                return "ARRAY";
            default:
                return "UNKNOWN";
            }
        }

    } // namespace engine
} // namespace scratchbird
