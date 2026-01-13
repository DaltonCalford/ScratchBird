/**
 * ScratchBird SBLR v2.0 - Semantic Analyzer Implementation
 *
 * See: include/scratchbird/sblr/semantic_analyzer_v2.h
 */

#include "scratchbird/sblr/semantic_analyzer_v2.h"
#include "scratchbird/catalog/virtual_catalog.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/domain_manager.h"
#include "scratchbird/parser/schema_path_v2.h"
#include "scratchbird/sblr/extract_element_catalog.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <cctype>
#include <unordered_set>

namespace scratchbird::parser::v2 {
using scratchbird::sblr::ElementArgSpec;
using scratchbird::sblr::ExtractField;
using scratchbird::sblr::extractFieldArgSpec;
using scratchbird::sblr::extractFieldToString;
using scratchbird::sblr::resolveExtractFieldName;

namespace {
core::CatalogManager::ObjectType toCatalogObjectType(DdlObjectType type) {
    return static_cast<core::CatalogManager::ObjectType>(static_cast<uint8_t>(type));
}

core::ObjectPath buildObjectPath(const SchemaPath& path, const StringPool& pool) {
    core::ObjectPath out;
    out.type = static_cast<core::PathType>(path.type);
    out.no_search_path = path.no_search_path;
    out.components.reserve(path.components.size());
    for (auto id : path.components) {
        out.components.emplace_back(pool.get(id));
    }
    return out;
}

SchemaPath appendPathComponent(const SchemaPath& base,
                               StringPool::StringId name,
                               SourceSpan span) {
    SchemaPath combined = base;
    combined.components.push_back(name);
    combined.span = span;
    return combined;
}

struct GroupByColumnKey {
    ID table_uuid{};
    uint32_t column_index = 0;

    bool operator==(const GroupByColumnKey& other) const {
        return table_uuid == other.table_uuid && column_index == other.column_index;
    }
};

struct GroupByColumnKeyHash {
    size_t operator()(const GroupByColumnKey& key) const noexcept {
        size_t h = 0;
        for (auto byte : key.table_uuid.bytes) {
            h = (h * 131) ^ static_cast<size_t>(byte);
        }
        h ^= std::hash<uint32_t>{}(key.column_index) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

bool typesEquivalent(const ResolvedType& a, const ResolvedType& b) {
    if (a.data_type != b.data_type ||
        a.is_domain != b.is_domain ||
        a.domain_id != b.domain_id ||
        a.is_array != b.is_array ||
        a.with_time_zone != b.with_time_zone) {
        return false;
    }

    return a.precision == b.precision &&
           a.scale == b.scale &&
           a.length == b.length &&
           a.array_size == b.array_size;
}

constexpr int32_t kDefaultVarcharLength = 255;
constexpr int32_t kDefaultDecimalPrecision = 18;
constexpr int32_t kDefaultDecimalScale = 0;

struct ElementInfo
{
    bool allowed = false;
    bool writable = false;
    ResolvedType type;
};

ResolvedType makeSimpleType(DataType type, bool nullable)
{
    ResolvedType out;
    out.data_type = type;
    out.is_nullable = nullable;
    return out;
}

ResolvedType makeTimeType(bool nullable, bool with_time_zone)
{
    ResolvedType out;
    out.data_type = DataType::TIME;
    out.is_nullable = nullable;
    out.with_time_zone = with_time_zone;
    return out;
}

ResolvedType makeTimestampType(bool nullable, bool with_time_zone)
{
    ResolvedType out;
    out.data_type = DataType::TIMESTAMP;
    out.is_nullable = nullable;
    out.with_time_zone = with_time_zone;
    return out;
}

ResolvedType makeArrayType(bool nullable)
{
    ResolvedType out;
    out.data_type = DataType::ARRAY;
    out.is_nullable = nullable;
    out.is_array = true;
    return out;
}

ElementInfo resolveElementInfo(const ResolvedType& source, ExtractField field)
{
    ElementInfo info;
    info.type = makeSimpleType(DataType::UNKNOWN, source.is_nullable);

    auto allow = [&](const ResolvedType& type, bool writable) {
        info.allowed = true;
        info.writable = writable;
        info.type = type;
    };

    auto allowSimple = [&](DataType type, bool writable) {
        allow(makeSimpleType(type, source.is_nullable), writable);
    };

    switch (source.data_type)
    {
        case DataType::UNKNOWN:
        case DataType::NULL_TYPE:
            if (field == ExtractField::VALUE)
            {
                allow(source, false);
            }
            return info;

        case DataType::DATE:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::YEAR:
                case ExtractField::MONTH:
                case ExtractField::DAY:
                    allowSimple(DataType::INT32, true);
                    break;
                case ExtractField::DOW:
                case ExtractField::DOY:
                case ExtractField::QUARTER:
                case ExtractField::WEEK:
                case ExtractField::ISO_WEEK:
                case ExtractField::ISO_YEAR:
                case ExtractField::ISO_DOW:
                case ExtractField::CENTURY:
                case ExtractField::DECADE:
                case ExtractField::MILLENNIUM:
                case ExtractField::EPOCH:
                    allowSimple(DataType::INT32, false);
                    if (field == ExtractField::EPOCH)
                    {
                        allowSimple(DataType::INT64, false);
                    }
                    break;
                case ExtractField::TIMEZONE:
                case ExtractField::TZ_OFFSET:
                case ExtractField::TIMEZONE_HOUR:
                case ExtractField::TIMEZONE_MINUTE:
                    allowSimple(DataType::INT32, true);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::TIME:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::HOUR:
                case ExtractField::MINUTE:
                case ExtractField::SECOND:
                case ExtractField::MILLISECOND:
                case ExtractField::MICROSECOND:
                    allowSimple(DataType::INT32, true);
                    break;
                case ExtractField::HOUR12:
                case ExtractField::EPOCH:
                    allowSimple(field == ExtractField::EPOCH ? DataType::INT64 : DataType::INT32,
                                false);
                    break;
                case ExtractField::TIMEZONE:
                case ExtractField::TZ_OFFSET:
                case ExtractField::TIMEZONE_HOUR:
                case ExtractField::TIMEZONE_MINUTE:
                    allowSimple(DataType::INT32, true);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::TIMESTAMP:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::YEAR:
                case ExtractField::MONTH:
                case ExtractField::DAY:
                case ExtractField::HOUR:
                case ExtractField::MINUTE:
                case ExtractField::SECOND:
                case ExtractField::MILLISECOND:
                case ExtractField::MICROSECOND:
                    allowSimple(DataType::INT32, true);
                    break;
                case ExtractField::DOW:
                case ExtractField::DOY:
                case ExtractField::QUARTER:
                case ExtractField::WEEK:
                case ExtractField::ISO_WEEK:
                case ExtractField::ISO_YEAR:
                case ExtractField::ISO_DOW:
                case ExtractField::CENTURY:
                case ExtractField::DECADE:
                case ExtractField::MILLENNIUM:
                    allowSimple(DataType::INT32, false);
                    break;
                case ExtractField::EPOCH:
                    allowSimple(DataType::INT64, false);
                    break;
                case ExtractField::TIMEZONE:
                case ExtractField::TZ_OFFSET:
                case ExtractField::TIMEZONE_HOUR:
                case ExtractField::TIMEZONE_MINUTE:
                    allowSimple(DataType::INT32, true);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::INTERVAL:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::YEAR:
                case ExtractField::MONTH:
                case ExtractField::DAY:
                case ExtractField::HOUR:
                case ExtractField::MINUTE:
                case ExtractField::SECOND:
                case ExtractField::MILLISECOND:
                case ExtractField::MICROSECOND:
                    allowSimple(DataType::INT32, true);
                    break;
                case ExtractField::TOTAL_MONTHS:
                    allowSimple(DataType::INT32, false);
                    break;
                case ExtractField::TOTAL_DAYS:
                    allowSimple(DataType::INT64, false);
                    break;
                case ExtractField::TOTAL_SECONDS:
                case ExtractField::EPOCH:
                    allowSimple(DataType::FLOAT64, false);
                    break;
                case ExtractField::SIGN:
                    allowSimple(DataType::INT8, false);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::INT8:
        case DataType::INT16:
        case DataType::INT32:
        case DataType::INT64:
        case DataType::INT128:
        case DataType::UINT8:
        case DataType::UINT16:
        case DataType::UINT32:
        case DataType::UINT64:
        case DataType::UINT128:
        {
            bool is_128 = (source.data_type == DataType::INT128 || source.data_type == DataType::UINT128);
            auto byte_width = [&]() -> int32_t {
                switch (source.data_type)
                {
                    case DataType::INT8: return 1;
                    case DataType::INT16: return 2;
                    case DataType::INT32: return 4;
                    case DataType::INT64: return 8;
                    case DataType::INT128: return 16;
                    case DataType::UINT8: return 1;
                    case DataType::UINT16: return 2;
                    case DataType::UINT32: return 4;
                    case DataType::UINT64: return 8;
                    case DataType::UINT128: return 16;
                    default: return 0;
                }
            };

            switch (field)
            {
                case ExtractField::VALUE:
                    allow(source, true);
                    break;
                case ExtractField::SIGN:
                    allowSimple(DataType::INT8, false);
                    break;
                case ExtractField::ABS:
                    allow(source, false);
                    break;
                case ExtractField::BYTES:
                    allowSimple(DataType::INT16, false);
                    break;
                case ExtractField::BITS:
                    allowSimple(DataType::INT16, false);
                    break;
                case ExtractField::HI64:
                case ExtractField::LO64:
                    if (is_128)
                    {
                        allowSimple(DataType::UINT64, false);
                    }
                    break;
                default:
                    break;
            }
            (void)byte_width;
            return info;
        }

        case DataType::FLOAT32:
        case DataType::FLOAT64:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::SIGN: allowSimple(DataType::INT8, false); break;
                case ExtractField::EXPONENT: allowSimple(DataType::INT32, false); break;
                case ExtractField::MANTISSA: allowSimple(DataType::INT64, false); break;
                case ExtractField::IS_NAN:
                case ExtractField::IS_INF:
                    allowSimple(DataType::BOOLEAN, false);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::DECIMAL:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::PRECISION:
                case ExtractField::SCALE:
                    allowSimple(DataType::INT16, false);
                    break;
                case ExtractField::UNSCALED:
                    allowSimple(DataType::INT128, true);
                    break;
                case ExtractField::SIGN:
                    allowSimple(DataType::INT8, false);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::MONEY:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::SCALE: allowSimple(DataType::INT8, false); break;
                case ExtractField::MAJOR: allowSimple(DataType::INT64, false); break;
                case ExtractField::MINOR: allowSimple(DataType::INT32, false); break;
                case ExtractField::SIGN: allowSimple(DataType::INT8, false); break;
                default: break;
            }
            return info;
        }

        case DataType::BOOLEAN:
        {
            if (field == ExtractField::VALUE)
            {
                allow(source, true);
            }
            return info;
        }

        case DataType::CHAR:
        case DataType::VARCHAR:
        case DataType::TEXT:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::CHAR_LENGTH:
                case ExtractField::OCTET_LENGTH:
                case ExtractField::CODEPOINT_LENGTH:
                    allowSimple(DataType::INT32, false);
                    break;
                case ExtractField::TRIMMED_LENGTH:
                    if (source.data_type == DataType::CHAR)
                    {
                        allowSimple(DataType::INT32, false);
                    }
                    break;
                default: break;
            }
            return info;
        }

        case DataType::JSON:
        case DataType::JSONB:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::PATH: allow(source, true); break;
                case ExtractField::TYPE: allowSimple(DataType::TEXT, false); break;
                case ExtractField::KEYS: allow(makeArrayType(source.is_nullable), false); break;
                default: break;
            }
            return info;
        }

        case DataType::XML:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::PATH: allow(source, true); break;
                case ExtractField::ATTRIBUTES:
                    allow(makeArrayType(source.is_nullable), false);
                    break;
                default: break;
            }
            return info;
        }

        case DataType::BINARY:
        case DataType::VARBINARY:
        case DataType::BLOB:
        case DataType::BYTEA:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::LENGTH: allowSimple(DataType::INT32, false); break;
                case ExtractField::BYTE:
                case ExtractField::BIT:
                    allowSimple(DataType::UINT8, true);
                    break;
                case ExtractField::SLICE: allow(source, true); break;
                case ExtractField::DIGEST: allowSimple(DataType::BINARY, false); break;
                default: break;
            }
            return info;
        }

        case DataType::VECTOR:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::DIMENSION: allowSimple(DataType::INT32, false); break;
                case ExtractField::ELEMENT: allowSimple(DataType::FLOAT32, true); break;
                case ExtractField::NORM_L2:
                case ExtractField::DOT:
                    allowSimple(DataType::FLOAT64, false);
                    break;
                default: break;
            }
            return info;
        }

        case DataType::UUID:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::BYTES: allowSimple(DataType::BINARY, true); break;
                case ExtractField::VERSION:
                case ExtractField::VARIANT:
                case ExtractField::CLOCK_SEQ:
                    allowSimple(DataType::INT32, false);
                    break;
                case ExtractField::TIMESTAMP:
                    allowSimple(DataType::INT64, false);
                    break;
                case ExtractField::NODE:
                    allowSimple(DataType::TEXT, false);
                    break;
                case ExtractField::TIME_LOW:
                    allowSimple(DataType::UINT32, false);
                    break;
                case ExtractField::TIME_MID:
                case ExtractField::TIME_HIGH:
                    allowSimple(DataType::UINT16, false);
                    break;
                case ExtractField::RAND_A:
                    allowSimple(DataType::UINT32, false);
                    break;
                case ExtractField::RAND_B:
                    allowSimple(DataType::BINARY, false);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::POINT:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::X:
                case ExtractField::Y:
                    allowSimple(DataType::FLOAT64, true);
                    break;
                case ExtractField::SRID:
                    allowSimple(DataType::INT32, true);
                    break;
                case ExtractField::BBOX:
                    allowSimple(DataType::POLYGON, false);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::LINESTRING:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::SRID:
                    allowSimple(DataType::INT32, true);
                    break;
                case ExtractField::NUM_POINTS:
                    allowSimple(DataType::INT32, false);
                    break;
                case ExtractField::POINTS:
                    allow(makeArrayType(source.is_nullable), false);
                    break;
                case ExtractField::START_POINT:
                case ExtractField::END_POINT:
                    allowSimple(DataType::POINT, false);
                    break;
                case ExtractField::LENGTH:
                    allowSimple(DataType::FLOAT64, false);
                    break;
                case ExtractField::BBOX:
                    allowSimple(DataType::POLYGON, false);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::POLYGON:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::SRID:
                    allowSimple(DataType::INT32, true);
                    break;
                case ExtractField::NUM_RINGS:
                    allowSimple(DataType::INT32, false);
                    break;
                case ExtractField::EXTERIOR_RING:
                    allowSimple(DataType::LINESTRING, false);
                    break;
                case ExtractField::RINGS:
                    allow(makeArrayType(source.is_nullable), false);
                    break;
                case ExtractField::AREA:
                    allowSimple(DataType::FLOAT64, false);
                    break;
                case ExtractField::BBOX:
                    allowSimple(DataType::POLYGON, false);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::MULTIPOINT:
        case DataType::MULTILINESTRING:
        case DataType::MULTIPOLYGON:
        case DataType::GEOMETRYCOLLECTION:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::SRID:
                    allowSimple(DataType::INT32, true);
                    break;
                case ExtractField::NUM_GEOMETRIES:
                    allowSimple(DataType::INT32, false);
                    break;
                case ExtractField::GEOMETRIES:
                    allow(makeArrayType(source.is_nullable), false);
                    break;
                case ExtractField::BBOX:
                    allowSimple(DataType::POLYGON, false);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::ARRAY:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::ELEMENT:
                    allowSimple(DataType::VARIANT, true);
                    break;
                case ExtractField::CARDINALITY:
                case ExtractField::NDIMS:
                case ExtractField::LOWER:
                case ExtractField::UPPER:
                    allowSimple(DataType::INT32, false);
                    break;
                case ExtractField::DIMS:
                    allow(makeArrayType(source.is_nullable), false);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::COMPOSITE:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::FIELD:
                    allowSimple(DataType::VARIANT, true);
                    break;
                case ExtractField::FIELD_NAMES:
                    allow(makeArrayType(source.is_nullable), false);
                    break;
                case ExtractField::CARDINALITY:
                    allowSimple(DataType::INT32, false);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::VARIANT:
        {
            switch (field)
            {
                case ExtractField::VALUE:
                    allow(source, true);
                    break;
                case ExtractField::DATATYPE:
                    allowSimple(DataType::INT32, true);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::TSVECTOR:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::LEXEMES:
                case ExtractField::POSITIONS:
                case ExtractField::WEIGHTS:
                    allow(makeArrayType(source.is_nullable), false);
                    break;
                case ExtractField::SIZE:
                    allowSimple(DataType::INT32, false);
                    break;
                case ExtractField::HAS_LEXEME:
                    allowSimple(DataType::BOOLEAN, false);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::TSQUERY:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::ROOT_OP:
                    allowSimple(DataType::TEXT, false);
                    break;
                case ExtractField::TERMS:
                case ExtractField::OPERATORS:
                    allow(makeArrayType(source.is_nullable), false);
                    break;
                case ExtractField::PHRASE_DISTANCE:
                case ExtractField::NODES:
                    allowSimple(DataType::INT32, false);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::INT4RANGE:
        case DataType::INT8RANGE:
        case DataType::NUMRANGE:
        case DataType::DATERANGE:
        case DataType::TSRANGE:
        case DataType::TSTZRANGE:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::LOWER:
                case ExtractField::UPPER:
                case ExtractField::LOWER_VALUE:
                case ExtractField::UPPER_VALUE:
                {
                    if (source.data_type == DataType::INT4RANGE)
                    {
                        allowSimple(DataType::INT32, true);
                    }
                    else if (source.data_type == DataType::INT8RANGE)
                    {
                        allowSimple(DataType::INT64, true);
                    }
                    else if (source.data_type == DataType::NUMRANGE)
                    {
                        allowSimple(DataType::FLOAT64, true);
                    }
                    else if (source.data_type == DataType::DATERANGE)
                    {
                        allowSimple(DataType::DATE, true);
                    }
                    else if (source.data_type == DataType::TSRANGE)
                    {
                        allow(makeTimestampType(source.is_nullable, false), true);
                    }
                    else if (source.data_type == DataType::TSTZRANGE)
                    {
                        allow(makeTimestampType(source.is_nullable, true), true);
                    }
                    break;
                }
                case ExtractField::LOWER_INC:
                case ExtractField::UPPER_INC:
                case ExtractField::LOWER_INF:
                case ExtractField::UPPER_INF:
                case ExtractField::ISEMPTY:
                    allowSimple(DataType::BOOLEAN, true);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::INET:
        case DataType::CIDR:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::FAMILY:
                case ExtractField::NETMASK:
                    allowSimple(DataType::INT32, field == ExtractField::NETMASK);
                    break;
                case ExtractField::ADDRESS:
                    allowSimple(DataType::TEXT, true);
                    break;
                case ExtractField::NETWORK:
                case ExtractField::NETMASK_ADDR:
                case ExtractField::HOSTMASK:
                    allow(source, false);
                    break;
                case ExtractField::BROADCAST:
                    if (source.data_type == DataType::INET)
                    {
                        allow(source, false);
                    }
                    break;
                case ExtractField::IS_IPV4:
                case ExtractField::IS_IPV6:
                    allowSimple(DataType::BOOLEAN, false);
                    break;
                default:
                    break;
            }
            return info;
        }

        case DataType::MACADDR:
        case DataType::MACADDR8:
        {
            switch (field)
            {
                case ExtractField::VALUE: allow(source, true); break;
                case ExtractField::BYTES:
                case ExtractField::OUI:
                case ExtractField::VENDOR:
                case ExtractField::NIC:
                    allowSimple(DataType::BINARY, true);
                    break;
                case ExtractField::TRUNC:
                    allowSimple(DataType::BINARY, false);
                    break;
                case ExtractField::IS_MULTICAST:
                case ExtractField::IS_LOCAL:
                    allowSimple(DataType::BOOLEAN, false);
                    break;
                default:
                    break;
            }
            return info;
        }

        default:
            return info;
    }
}
constexpr size_t kMaxDomainInheritanceDepth = 10;

core::DomainType toCoreDomainType(DomainKind kind) {
    switch (kind) {
        case DomainKind::BASIC:
            return core::DomainType::BASIC;
        case DomainKind::RECORD:
            return core::DomainType::RECORD;
        case DomainKind::ENUM:
            return core::DomainType::ENUM;
        case DomainKind::SET:
            return core::DomainType::SET;
        case DomainKind::VARIANT:
            return core::DomainType::VARIANT;
    }
    return core::DomainType::BASIC;
}

bool baseTypeMatchesParent(const ResolvedType& child, const core::DomainInfo& parent) {
    if (child.data_type != parent.base_type) {
        return false;
    }
    switch (child.data_type) {
        case DataType::VARCHAR:
        case DataType::CHAR: {
            int32_t length = child.length.value_or(kDefaultVarcharLength);
            return parent.precision == static_cast<uint32_t>(length);
        }
        case DataType::DECIMAL: {
            int32_t precision = child.precision.value_or(kDefaultDecimalPrecision);
            int32_t scale = child.scale.value_or(kDefaultDecimalScale);
            return parent.precision == static_cast<uint32_t>(precision) &&
                   parent.scale == static_cast<uint32_t>(scale);
        }
        default:
            return true;
    }
}

bool schemaPathEquals(const SchemaPath& a, const SchemaPath& b, const StringPool& pool) {
    if (a.components.size() != b.components.size()) {
        return false;
    }
    for (size_t i = 0; i < a.components.size(); ++i) {
        std::string left = core::IdentifierUtils::toUpper(std::string(pool.get(a.components[i])));
        std::string right = core::IdentifierUtils::toUpper(std::string(pool.get(b.components[i])));
        if (left != right) {
            return false;
        }
    }
    return true;
}

enum class LiteralKind {
    UNKNOWN,
    NULL_LITERAL,
    BOOLEAN,
    NUMBER,
    STRING
};

std::string trimString(std::string_view input) {
    size_t start = 0;
    while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start]))) {
        start++;
    }
    if (start == input.size()) {
        return {};
    }
    size_t end = input.size() - 1;
    while (end > start && std::isspace(static_cast<unsigned char>(input[end]))) {
        end--;
    }
    return std::string(input.substr(start, end - start + 1));
}

bool equalsIgnoreCase(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool isWordChar(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool hasTokenOutsideQuotes(const std::string& expr, std::string_view token) {
    bool in_string = false;
    for (size_t i = 0; i < expr.size();) {
        char c = expr[i];
        if (in_string) {
            if (c == '\'') {
                if (i + 1 < expr.size() && expr[i + 1] == '\'') {
                    i += 2;
                    continue;
                }
                in_string = false;
            }
            ++i;
            continue;
        }
        if (c == '\'') {
            in_string = true;
            ++i;
            continue;
        }
        if (isWordChar(c)) {
            size_t start = i;
            while (i < expr.size() && isWordChar(expr[i])) {
                ++i;
            }
            std::string_view tok(expr.data() + start, i - start);
            if (equalsIgnoreCase(tok, token)) {
                return true;
            }
            continue;
        }
        ++i;
    }
    return false;
}

std::string normalizeValueToken(const std::string& expr) {
    std::string out;
    out.reserve(expr.size());
    bool in_string = false;
    for (size_t i = 0; i < expr.size();) {
        char c = expr[i];
        if (in_string) {
            out.push_back(c);
            if (c == '\'') {
                if (i + 1 < expr.size() && expr[i + 1] == '\'') {
                    out.push_back(expr[i + 1]);
                    i += 2;
                    continue;
                }
                in_string = false;
            }
            ++i;
            continue;
        }
        if (c == '\'') {
            in_string = true;
            out.push_back(c);
            ++i;
            continue;
        }
        if (isWordChar(c)) {
            size_t start = i;
            while (i < expr.size() && isWordChar(expr[i])) {
                ++i;
            }
            std::string_view tok(expr.data() + start, i - start);
            if (equalsIgnoreCase(tok, "VALUE")) {
                out.append("VALUE");
            } else {
                out.append(tok);
            }
            continue;
        }
        out.push_back(c);
        ++i;
    }
    return out;
}

bool isNumericLiteral(std::string_view input) {
    if (input.empty()) {
        return false;
    }
    size_t i = 0;
    if (input[i] == '+' || input[i] == '-') {
        ++i;
    }
    bool saw_digit = false;
    bool saw_dot = false;
    bool saw_exp = false;

    for (; i < input.size(); ++i) {
        char c = input[i];
        if (std::isdigit(static_cast<unsigned char>(c))) {
            saw_digit = true;
            continue;
        }
        if (c == '.' && !saw_dot && !saw_exp) {
            saw_dot = true;
            continue;
        }
        if ((c == 'e' || c == 'E') && saw_digit && !saw_exp) {
            saw_exp = true;
            saw_digit = false;
            if (i + 1 < input.size() && (input[i + 1] == '+' || input[i + 1] == '-')) {
                ++i;
            }
            continue;
        }
        return false;
    }
    return saw_digit;
}

LiteralKind classifyLiteral(const std::string& expr) {
    std::string trimmed = trimString(expr);
    if (trimmed.empty()) {
        return LiteralKind::UNKNOWN;
    }
    if (equalsIgnoreCase(trimmed, "NULL")) {
        return LiteralKind::NULL_LITERAL;
    }
    if (equalsIgnoreCase(trimmed, "TRUE") || equalsIgnoreCase(trimmed, "FALSE")) {
        return LiteralKind::BOOLEAN;
    }
    if (trimmed.size() >= 2 && trimmed.front() == '\'' && trimmed.back() == '\'') {
        return LiteralKind::STRING;
    }
    if (isNumericLiteral(trimmed)) {
        return LiteralKind::NUMBER;
    }
    return LiteralKind::UNKNOWN;
}

bool isNumericType(DataType type) {
    switch (type) {
        case DataType::INT8:
        case DataType::INT16:
        case DataType::INT32:
        case DataType::INT64:
        case DataType::INT128:
        case DataType::UINT8:
        case DataType::UINT16:
        case DataType::UINT32:
        case DataType::UINT64:
        case DataType::UINT128:
        case DataType::FLOAT32:
        case DataType::FLOAT64:
        case DataType::DECIMAL:
        case DataType::MONEY:
            return true;
        default:
            return false;
    }
}

bool literalCompatibleWithType(LiteralKind kind, DataType type) {
    if (kind == LiteralKind::UNKNOWN || kind == LiteralKind::NULL_LITERAL) {
        return true;
    }
    if (kind == LiteralKind::BOOLEAN) {
        return type == DataType::BOOLEAN;
    }
    if (kind == LiteralKind::NUMBER) {
        return isNumericType(type);
    }
    if (kind == LiteralKind::STRING) {
        return !isNumericType(type) && type != DataType::BOOLEAN;
    }
    return true;
}

bool expressionsEqual(const ResolvedExpression* left, const ResolvedExpression* right);

bool expressionListEqual(const std::vector<ResolvedExpression*>& left,
                         const std::vector<ResolvedExpression*>& right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (size_t i = 0; i < left.size(); ++i) {
        if (!expressionsEqual(left[i], right[i])) {
            return false;
        }
    }
    return true;
}

bool expressionsEqual(const ResolvedExpression* left, const ResolvedExpression* right) {
    if (left == right) {
        return true;
    }
    if (!left || !right) {
        return false;
    }

    if (auto* lit_left = dynamic_cast<const ResolvedLiteral*>(left)) {
        auto* lit_right = dynamic_cast<const ResolvedLiteral*>(right);
        if (!lit_right) {
            return false;
        }
        if (lit_left->literal_type != lit_right->literal_type ||
            lit_left->is_null != lit_right->is_null ||
            lit_left->is_default != lit_right->is_default) {
            return false;
        }
        switch (lit_left->literal_type) {
            case LiteralType::INTEGER:
                return lit_left->int_value == lit_right->int_value;
            case LiteralType::FLOAT:
                return lit_left->float_value == lit_right->float_value;
            case LiteralType::BOOLEAN:
                return lit_left->bool_value == lit_right->bool_value;
            case LiteralType::STRING:
            case LiteralType::BLOB:
                return lit_left->string_value == lit_right->string_value;
            case LiteralType::NULL_VALUE:
            case LiteralType::DEFAULT:
                return true;
        }
        return false;
    }

    if (auto* col_left = dynamic_cast<const ResolvedColumnRefExpr*>(left)) {
        auto* col_right = dynamic_cast<const ResolvedColumnRefExpr*>(right);
        if (!col_right) {
            return false;
        }
        return col_left->column.table_uuid == col_right->column.table_uuid &&
               col_left->column.column_index == col_right->column.column_index;
    }

    if (auto* bin_left = dynamic_cast<const ResolvedBinaryExpr*>(left)) {
        auto* bin_right = dynamic_cast<const ResolvedBinaryExpr*>(right);
        if (!bin_right || bin_left->op != bin_right->op) {
            return false;
        }
        return expressionsEqual(bin_left->left, bin_right->left) &&
               expressionsEqual(bin_left->right, bin_right->right);
    }

    if (auto* unary_left = dynamic_cast<const ResolvedUnaryExpr*>(left)) {
        auto* unary_right = dynamic_cast<const ResolvedUnaryExpr*>(right);
        if (!unary_right || unary_left->op != unary_right->op) {
            return false;
        }
        return expressionsEqual(unary_left->operand, unary_right->operand);
    }

    if (auto* cast_left = dynamic_cast<const ResolvedCast*>(left)) {
        auto* cast_right = dynamic_cast<const ResolvedCast*>(right);
        if (!cast_right || !typesEquivalent(cast_left->target_type, cast_right->target_type)) {
            return false;
        }
        return expressionsEqual(cast_left->expr, cast_right->expr);
    }

    if (auto* func_left = dynamic_cast<const ResolvedFunctionCall*>(left)) {
        auto* func_right = dynamic_cast<const ResolvedFunctionCall*>(right);
        if (!func_right) {
            return false;
        }
        if (func_left->function.function_uuid != func_right->function.function_uuid ||
            func_left->function.function_name != func_right->function.function_name ||
            func_left->distinct != func_right->distinct ||
            func_left->is_window != func_right->is_window) {
            return false;
        }
        if (!expressionListEqual(func_left->arguments, func_right->arguments)) {
            return false;
        }
        if (!expressionsEqual(func_left->filter, func_right->filter)) {
            return false;
        }
        if (!expressionsEqual(func_left->separator, func_right->separator)) {
            return false;
        }
        return true;
    }

    if (auto* between_left = dynamic_cast<const ResolvedBetweenExpr*>(left)) {
        auto* between_right = dynamic_cast<const ResolvedBetweenExpr*>(right);
        if (!between_right ||
            between_left->negated != between_right->negated ||
            between_left->symmetric != between_right->symmetric) {
            return false;
        }
        return expressionsEqual(between_left->expr, between_right->expr) &&
               expressionsEqual(between_left->low, between_right->low) &&
               expressionsEqual(between_left->high, between_right->high);
    }

    if (auto* like_left = dynamic_cast<const ResolvedLikeExpr*>(left)) {
        auto* like_right = dynamic_cast<const ResolvedLikeExpr*>(right);
        if (!like_right ||
            like_left->negated != like_right->negated ||
            like_left->case_insensitive != like_right->case_insensitive ||
            like_left->match_kind != like_right->match_kind) {
            return false;
        }
        return expressionsEqual(like_left->expr, like_right->expr) &&
               expressionsEqual(like_left->pattern, like_right->pattern) &&
               expressionsEqual(like_left->escape, like_right->escape);
    }

    if (auto* is_null_left = dynamic_cast<const ResolvedIsNullExpr*>(left)) {
        auto* is_null_right = dynamic_cast<const ResolvedIsNullExpr*>(right);
        if (!is_null_right || is_null_left->negated != is_null_right->negated) {
            return false;
        }
        return expressionsEqual(is_null_left->expr, is_null_right->expr);
    }

    if (auto* in_left = dynamic_cast<const ResolvedInExpr*>(left)) {
        auto* in_right = dynamic_cast<const ResolvedInExpr*>(right);
        if (!in_right || in_left->negated != in_right->negated ||
            in_left->has_subquery != in_right->has_subquery) {
            return false;
        }
        if (!expressionsEqual(in_left->expr, in_right->expr)) {
            return false;
        }
        if (in_left->has_subquery) {
            return false;
        }
        return expressionListEqual(in_left->values, in_right->values);
    }

    if (auto* arr_left = dynamic_cast<const ResolvedArrayExpr*>(left)) {
        auto* arr_right = dynamic_cast<const ResolvedArrayExpr*>(right);
        if (!arr_right || arr_left->has_subquery != arr_right->has_subquery) {
            return false;
        }
        if (arr_left->has_subquery) {
            return false;
        }
        return expressionListEqual(arr_left->elements, arr_right->elements);
    }

    if (auto* case_left = dynamic_cast<const ResolvedCase*>(left)) {
        auto* case_right = dynamic_cast<const ResolvedCase*>(right);
        if (!case_right) {
            return false;
        }
        if (!expressionsEqual(case_left->operand, case_right->operand)) {
            return false;
        }
        if (case_left->when_clauses.size() != case_right->when_clauses.size()) {
            return false;
        }
        for (size_t i = 0; i < case_left->when_clauses.size(); ++i) {
            if (!expressionsEqual(case_left->when_clauses[i].when_expr,
                                  case_right->when_clauses[i].when_expr) ||
                !expressionsEqual(case_left->when_clauses[i].then_expr,
                                  case_right->when_clauses[i].then_expr)) {
                return false;
            }
        }
        return expressionsEqual(case_left->else_expr, case_right->else_expr);
    }

    return false;
}

bool containsAggregateExpr(const ResolvedExpression* expr) {
    if (!expr) {
        return false;
    }

    if (auto* func = dynamic_cast<const ResolvedFunctionCall*>(expr)) {
        if (func->function.is_aggregate) {
            return true;
        }
        for (auto* arg : func->arguments) {
            if (containsAggregateExpr(arg)) {
                return true;
            }
        }
        if (containsAggregateExpr(func->filter) ||
            containsAggregateExpr(func->separator)) {
            return true;
        }
        return false;
    }

    if (auto* bin = dynamic_cast<const ResolvedBinaryExpr*>(expr)) {
        return containsAggregateExpr(bin->left) || containsAggregateExpr(bin->right);
    }
    if (auto* unary = dynamic_cast<const ResolvedUnaryExpr*>(expr)) {
        return containsAggregateExpr(unary->operand);
    }
    if (auto* cast_expr = dynamic_cast<const ResolvedCast*>(expr)) {
        return containsAggregateExpr(cast_expr->expr);
    }
    if (auto* between_expr = dynamic_cast<const ResolvedBetweenExpr*>(expr)) {
        return containsAggregateExpr(between_expr->expr) ||
               containsAggregateExpr(between_expr->low) ||
               containsAggregateExpr(between_expr->high);
    }
    if (auto* like_expr = dynamic_cast<const ResolvedLikeExpr*>(expr)) {
        return containsAggregateExpr(like_expr->expr) ||
               containsAggregateExpr(like_expr->pattern) ||
               containsAggregateExpr(like_expr->escape);
    }
    if (auto* in_expr = dynamic_cast<const ResolvedInExpr*>(expr)) {
        if (containsAggregateExpr(in_expr->expr)) {
            return true;
        }
        for (auto* val : in_expr->values) {
            if (containsAggregateExpr(val)) {
                return true;
            }
        }
        return false;
    }
    if (auto* is_null_expr = dynamic_cast<const ResolvedIsNullExpr*>(expr)) {
        return containsAggregateExpr(is_null_expr->expr);
    }
    if (auto* case_expr = dynamic_cast<const ResolvedCase*>(expr)) {
        if (containsAggregateExpr(case_expr->operand)) {
            return true;
        }
        for (const auto& clause : case_expr->when_clauses) {
            if (containsAggregateExpr(clause.when_expr) ||
                containsAggregateExpr(clause.then_expr)) {
                return true;
            }
        }
        return containsAggregateExpr(case_expr->else_expr);
    }
    if (auto* arr_expr = dynamic_cast<const ResolvedArrayExpr*>(expr)) {
        for (auto* elem : arr_expr->elements) {
            if (containsAggregateExpr(elem)) {
                return true;
            }
        }
        return false;
    }

    return false;
}

bool isConstantExpr(const ResolvedExpression* expr) {
    if (!expr) {
        return true;
    }
    if (dynamic_cast<const ResolvedLiteral*>(expr)) {
        return true;
    }
    if (auto* bin = dynamic_cast<const ResolvedBinaryExpr*>(expr)) {
        return isConstantExpr(bin->left) && isConstantExpr(bin->right);
    }
    if (auto* unary = dynamic_cast<const ResolvedUnaryExpr*>(expr)) {
        return isConstantExpr(unary->operand);
    }
    if (auto* cast_expr = dynamic_cast<const ResolvedCast*>(expr)) {
        return isConstantExpr(cast_expr->expr);
    }
    if (auto* between_expr = dynamic_cast<const ResolvedBetweenExpr*>(expr)) {
        return isConstantExpr(between_expr->expr) &&
               isConstantExpr(between_expr->low) &&
               isConstantExpr(between_expr->high);
    }
    if (auto* like_expr = dynamic_cast<const ResolvedLikeExpr*>(expr)) {
        return isConstantExpr(like_expr->expr) &&
               isConstantExpr(like_expr->pattern) &&
               isConstantExpr(like_expr->escape);
    }
    if (auto* is_null_expr = dynamic_cast<const ResolvedIsNullExpr*>(expr)) {
        return isConstantExpr(is_null_expr->expr);
    }
    if (auto* in_expr = dynamic_cast<const ResolvedInExpr*>(expr)) {
        if (!isConstantExpr(in_expr->expr)) {
            return false;
        }
        for (auto* val : in_expr->values) {
            if (!isConstantExpr(val)) {
                return false;
            }
        }
        return true;
    }
    if (auto* case_expr = dynamic_cast<const ResolvedCase*>(expr)) {
        if (!isConstantExpr(case_expr->operand)) {
            return false;
        }
        for (const auto& clause : case_expr->when_clauses) {
            if (!isConstantExpr(clause.when_expr) ||
                !isConstantExpr(clause.then_expr)) {
                return false;
            }
        }
        return isConstantExpr(case_expr->else_expr);
    }
    return false;
}

bool containsColumnRef(const ResolvedExpression* expr) {
    if (!expr) {
        return false;
    }

    if (dynamic_cast<const ResolvedColumnRefExpr*>(expr)) {
        return true;
    }
    if (auto* bin = dynamic_cast<const ResolvedBinaryExpr*>(expr)) {
        return containsColumnRef(bin->left) || containsColumnRef(bin->right);
    }
    if (auto* unary = dynamic_cast<const ResolvedUnaryExpr*>(expr)) {
        return containsColumnRef(unary->operand);
    }
    if (auto* cast_expr = dynamic_cast<const ResolvedCast*>(expr)) {
        return containsColumnRef(cast_expr->expr);
    }
    if (auto* between_expr = dynamic_cast<const ResolvedBetweenExpr*>(expr)) {
        return containsColumnRef(between_expr->expr) ||
               containsColumnRef(between_expr->low) ||
               containsColumnRef(between_expr->high);
    }
    if (auto* like_expr = dynamic_cast<const ResolvedLikeExpr*>(expr)) {
        return containsColumnRef(like_expr->expr) ||
               containsColumnRef(like_expr->pattern) ||
               containsColumnRef(like_expr->escape);
    }
    if (auto* in_expr = dynamic_cast<const ResolvedInExpr*>(expr)) {
        if (containsColumnRef(in_expr->expr)) {
            return true;
        }
        if (in_expr->has_subquery) {
            return false;
        }
        for (auto* val : in_expr->values) {
            if (containsColumnRef(val)) {
                return true;
            }
        }
        return false;
    }
    if (auto* is_null_expr = dynamic_cast<const ResolvedIsNullExpr*>(expr)) {
        return containsColumnRef(is_null_expr->expr);
    }
    if (auto* case_expr = dynamic_cast<const ResolvedCase*>(expr)) {
        if (containsColumnRef(case_expr->operand)) {
            return true;
        }
        for (const auto& clause : case_expr->when_clauses) {
            if (containsColumnRef(clause.when_expr) ||
                containsColumnRef(clause.then_expr)) {
                return true;
            }
        }
        return containsColumnRef(case_expr->else_expr);
    }
    if (auto* func = dynamic_cast<const ResolvedFunctionCall*>(expr)) {
        for (auto* arg : func->arguments) {
            if (containsColumnRef(arg)) {
                return true;
            }
        }
        if (containsColumnRef(func->filter) || containsColumnRef(func->separator)) {
            return true;
        }
        return false;
    }

    return false;
}

const ResolvedColumnRefExpr* findUngroupedColumn(
    const ResolvedExpression* expr,
    const std::unordered_set<GroupByColumnKey, GroupByColumnKeyHash>& grouped) {
    if (!expr) {
        return nullptr;
    }

    if (auto* col = dynamic_cast<const ResolvedColumnRefExpr*>(expr)) {
        GroupByColumnKey key{col->column.table_uuid, col->column.column_index};
        if (grouped.find(key) == grouped.end()) {
            return col;
        }
        return nullptr;
    }

    if (auto* bin = dynamic_cast<const ResolvedBinaryExpr*>(expr)) {
        if (auto* found = findUngroupedColumn(bin->left, grouped)) {
            return found;
        }
        return findUngroupedColumn(bin->right, grouped);
    }
    if (auto* unary = dynamic_cast<const ResolvedUnaryExpr*>(expr)) {
        return findUngroupedColumn(unary->operand, grouped);
    }
    if (auto* cast_expr = dynamic_cast<const ResolvedCast*>(expr)) {
        return findUngroupedColumn(cast_expr->expr, grouped);
    }
    if (auto* between_expr = dynamic_cast<const ResolvedBetweenExpr*>(expr)) {
        if (auto* found = findUngroupedColumn(between_expr->expr, grouped)) {
            return found;
        }
        if (auto* found = findUngroupedColumn(between_expr->low, grouped)) {
            return found;
        }
        return findUngroupedColumn(between_expr->high, grouped);
    }
    if (auto* like_expr = dynamic_cast<const ResolvedLikeExpr*>(expr)) {
        if (auto* found = findUngroupedColumn(like_expr->expr, grouped)) {
            return found;
        }
        if (auto* found = findUngroupedColumn(like_expr->pattern, grouped)) {
            return found;
        }
        return findUngroupedColumn(like_expr->escape, grouped);
    }
    if (auto* in_expr = dynamic_cast<const ResolvedInExpr*>(expr)) {
        if (auto* found = findUngroupedColumn(in_expr->expr, grouped)) {
            return found;
        }
        if (in_expr->has_subquery) {
            return nullptr;
        }
        for (auto* val : in_expr->values) {
            if (auto* found = findUngroupedColumn(val, grouped)) {
                return found;
            }
        }
        return nullptr;
    }
    if (auto* is_null_expr = dynamic_cast<const ResolvedIsNullExpr*>(expr)) {
        return findUngroupedColumn(is_null_expr->expr, grouped);
    }
    if (auto* case_expr = dynamic_cast<const ResolvedCase*>(expr)) {
        if (auto* found = findUngroupedColumn(case_expr->operand, grouped)) {
            return found;
        }
        for (const auto& clause : case_expr->when_clauses) {
            if (auto* found = findUngroupedColumn(clause.when_expr, grouped)) {
                return found;
            }
            if (auto* found = findUngroupedColumn(clause.then_expr, grouped)) {
                return found;
            }
        }
        return findUngroupedColumn(case_expr->else_expr, grouped);
    }
    if (auto* func = dynamic_cast<const ResolvedFunctionCall*>(expr)) {
        for (auto* arg : func->arguments) {
            if (auto* found = findUngroupedColumn(arg, grouped)) {
                return found;
            }
        }
        if (auto* found = findUngroupedColumn(func->filter, grouped)) {
            return found;
        }
        return findUngroupedColumn(func->separator, grouped);
    }

    return nullptr;
}

std::string stripRootPrefixForDisplay(const std::string& schema_path) {
    if (schema_path.empty()) {
        return schema_path;
    }

    size_t dot_pos = schema_path.find('.');
    std::string first_component =
        dot_pos == std::string::npos ? schema_path : schema_path.substr(0, dot_pos);
    if (core::IdentifierUtils::namesMatch(first_component, false /*search_delimited*/,
                                          "root", false /*stored_delimited*/)) {
        if (dot_pos == std::string::npos) {
            return std::string();
        }
        return schema_path.substr(dot_pos + 1);
    }

    return schema_path;
}
} // namespace

// =============================================================================
// SemanticResult Implementation
// =============================================================================

void SemanticResult::addError(const SemanticError& error) {
    errors_.push_back(error);
}

void SemanticResult::addWarning(const SemanticError& warning) {
    warnings_.push_back(warning);
}

// =============================================================================
// ResolutionScope Implementation
// =============================================================================

void ResolutionScope::addTable(const TableEntry& entry) {
    size_t index = tables_.size();
    tables_.push_back(entry);
    if (entry.alias != StringPool::INVALID_ID) {
        table_map_[entry.alias] = index;
    }
}

const ResolutionScope::TableEntry* ResolutionScope::findTable(StringPool::StringId name) const {
    auto it = table_map_.find(name);
    if (it != table_map_.end()) {
        return &tables_[it->second];
    }
    return nullptr;
}

ResolutionScope::ColumnLookupResult ResolutionScope::findColumn(StringPool::StringId name) const {
    ColumnLookupResult result;

    for (const auto& table : tables_) {
        for (const auto& col : table.columns) {
            if (col.name == name) {
                if (result.column != nullptr) {
                    result.ambiguous = true;
                    return result;
                }
                result.table = &table;
                result.column = &col;
            }
        }
    }

    return result;
}

const ResolvedTableRef::ColumnInfo* ResolutionScope::findColumn(
    StringPool::StringId table_name,
    StringPool::StringId column_name) const
{
    const TableEntry* table = findTable(table_name);
    if (!table) {
        return nullptr;
    }

    for (const auto& col : table->columns) {
        if (col.name == column_name) {
            return &col;
        }
    }

    return nullptr;
}

void ResolutionScope::clear() {
    tables_.clear();
    table_map_.clear();
}

// =============================================================================
// ResolvedASTArena Implementation
// =============================================================================

ResolvedASTArena::ResolvedASTArena(size_t block_size)
    : current_block_(nullptr)
    , block_size_(block_size)
    , total_allocated_(0)
{
    current_block_ = allocateBlock(block_size_);
}

ResolvedASTArena::~ResolvedASTArena() {
    callDestructors();
    reset();
    if (current_block_) {
        std::free(current_block_->data);
        delete current_block_;
    }
}

ResolvedASTArena::Block* ResolvedASTArena::allocateBlock(size_t size) {
    Block* block = new Block;
    block->data = static_cast<char*>(std::malloc(size));
    block->size = size;
    block->used = 0;
    block->next = nullptr;
    return block;
}

void* ResolvedASTArena::allocate(size_t size, size_t alignment) {
    size_t current = reinterpret_cast<size_t>(current_block_->data + current_block_->used);
    size_t aligned = (current + alignment - 1) & ~(alignment - 1);
    size_t padding = aligned - current;

    if (current_block_->used + padding + size > current_block_->size) {
        size_t new_size = std::max(block_size_, size + alignment);
        Block* new_block = allocateBlock(new_size);
        new_block->next = current_block_;
        current_block_ = new_block;

        current = reinterpret_cast<size_t>(current_block_->data);
        aligned = (current + alignment - 1) & ~(alignment - 1);
        padding = aligned - current;
    }

    void* result = current_block_->data + current_block_->used + padding;
    current_block_->used += padding + size;
    total_allocated_ += size;

    return result;
}

void ResolvedASTArena::trackDestructor(std::function<void()> dtor) {
    destructors_.push_back(std::move(dtor));
}

void ResolvedASTArena::callDestructors() {
    for (auto it = destructors_.rbegin(); it != destructors_.rend(); ++it) {
        (*it)();
    }
    destructors_.clear();
}

void ResolvedASTArena::reset() {
    callDestructors();
    while (current_block_ && current_block_->next) {
        Block* next = current_block_->next;
        std::free(current_block_->data);
        delete current_block_;
        current_block_ = next;
    }
    if (current_block_) {
        current_block_->used = 0;
    }
    total_allocated_ = 0;
}

// =============================================================================
// ResolvedType Implementation
// =============================================================================

bool ResolvedType::isNumeric() const {
    switch (data_type) {
        case DataType::INT16:
        case DataType::INT32:
        case DataType::INT64:
        case DataType::FLOAT32:
        case DataType::FLOAT64:
        case DataType::DECIMAL:
            return true;
        default:
            return false;
    }
}

bool ResolvedType::isString() const {
    switch (data_type) {
        case DataType::VARCHAR:
        case DataType::TEXT:
        case DataType::CHAR:
            return true;
        default:
            return false;
    }
}

bool ResolvedType::isBoolean() const {
    return data_type == DataType::BOOLEAN;
}

bool ResolvedType::isTemporal() const {
    switch (data_type) {
        case DataType::DATE:
        case DataType::TIME:
        case DataType::TIMESTAMP:
        case DataType::INTERVAL:
            return true;
        default:
            return false;
    }
}

bool ResolvedType::isComparableTo(const ResolvedType& other) const {
    if (data_type == other.data_type) {
        return true;
    }
    if (isNumeric() && other.isNumeric()) {
        return true;
    }
    if (isString() && other.isString()) {
        return true;
    }
    if (isTemporal() && other.isTemporal()) {
        return true;
    }
    return false;
}

bool ResolvedType::isAssignableTo(const ResolvedType& target) const {
    if (data_type == target.data_type) {
        return true;
    }
    if (isNumeric() && target.isNumeric()) {
        return true;
    }
    if (isString() && target.isString()) {
        return true;
    }
    return false;
}

// =============================================================================
// SemanticAnalyzerV2 Implementation
// =============================================================================

namespace {
    bool isZeroUuidLocal(const ID& id) {
        for (auto b : id.bytes) { if (b != 0) return false; }
        return true;
    }
}

SemanticAnalyzerV2::SemanticAnalyzerV2(CatalogManager& catalog, StringPool& string_pool)
    : catalog_(catalog)
    , string_pool_(string_pool)
{
    auto* conn_ctx = core::ConnectionContext::getCurrent();
    if (conn_ctx)
    {
        current_schema_ = conn_ctx->getCurrentSchemaId();
        if (isZeroUuidLocal(current_schema_) && !conn_ctx->current_schema().empty())
        {
            CatalogManager::SchemaInfo schema_info;
            if (catalog_.getSchema(conn_ctx->current_schema(), schema_info) == Status::OK)
            {
                current_schema_ = schema_info.schema_id;
            }
        }

        search_path_.clear();
        for (const auto& schema_path : conn_ctx->search_path())
        {
            CatalogManager::SchemaInfo schema_info;
            if (catalog_.getSchema(schema_path, schema_info) == Status::OK)
            {
                search_path_.push_back(schema_info.schema_id);
            }
        }

        if (search_path_.empty() && !isZeroUuidLocal(current_schema_))
        {
            search_path_.push_back(current_schema_);
        }
    }

    if (search_path_.empty())
    {
        // Initialize with default public schema
        CatalogManager::SchemaInfo schema_info;
        if (catalog_.getSchema("public", schema_info) == Status::OK)
        {
            current_schema_ = schema_info.schema_id;
            search_path_.push_back(schema_info.schema_id);
        }
    }
}

SemanticAnalyzerV2::~SemanticAnalyzerV2() = default;

SemanticResult SemanticAnalyzerV2::analyze(Statement* stmt) {
    SemanticResult result;
    current_result_ = &result;
    result.setStringPool(&string_pool_);

    // Reset state
    scope_stack_.clear();
    in_aggregate_ = false;
    has_aggregates_ = false;
    subquery_depth_ = 0;

    // Analyze statement
    ResolvedStatement* resolved = analyzeStatement(stmt);
    result.setStatement(resolved);

    current_result_ = nullptr;
    return result;
}

// =============================================================================
// Error Handling
// =============================================================================

void SemanticAnalyzerV2::error(SourceSpan span, const std::string& message, const std::string& hint) {
    if (current_result_) {
        SemanticError err;
        err.span = span;
        err.message = message;
        err.hint = hint;
        err.severity = SemanticError::Severity::ERROR;
        current_result_->addError(err);
    }
}

void SemanticAnalyzerV2::warning(SourceSpan span, const std::string& message, const std::string& hint) {
    if (current_result_) {
        SemanticError warn;
        warn.span = span;
        warn.message = message;
        warn.hint = hint;
        warn.severity = SemanticError::Severity::WARNING;
        current_result_->addWarning(warn);
    }
}

// =============================================================================
// Scope Management
// =============================================================================

void SemanticAnalyzerV2::pushScope() {
    scope_stack_.emplace_back();
}

void SemanticAnalyzerV2::popScope() {
    if (!scope_stack_.empty()) {
        scope_stack_.pop_back();
    }
}

ResolutionScope& SemanticAnalyzerV2::currentScope() {
    if (scope_stack_.empty()) {
        scope_stack_.emplace_back();
    }
    return scope_stack_.back();
}

// =============================================================================
// Name Resolution
// =============================================================================

std::optional<ResolvedTableRef> SemanticAnalyzerV2::resolveTable(
    const SchemaPath& path, SourceSpan span, bool allow_search_path)
{
    if (path.components.empty())
    {
        error(span, "Invalid table reference: empty path");
        return std::nullopt;
    }

    std::vector<std::string> components;
    components.reserve(path.components.size());
    for (auto id : path.components)
    {
        components.emplace_back(string_pool_.get(id));
    }

    auto join_components = [&](size_t count) -> std::string {
        std::string out;
        for (size_t i = 0; i < count; ++i)
        {
            if (i > 0)
            {
                out += ".";
            }
            out += components[i];
        }
        return out;
    };

    auto is_object_resolver = [&](const std::vector<std::string>& comps) -> bool {
        if (comps.size() != 3)
        {
            return false;
        }
        return core::IdentifierUtils::toUpper(comps[0]) == "SYS" &&
               core::IdentifierUtils::toUpper(comps[1]) == "CATALOG" &&
               core::IdentifierUtils::toUpper(comps[2]) == "OBJECT_RESOLVER";
    };

    if (path.type == PathType::ABSOLUTE && is_object_resolver(components))
    {
        ResolvedTableRef ref;
        ref.table_uuid = ID{};
        ref.schema_uuid = ID{};
        ref.name = internString("sys.catalog.object_resolver");
        ref.object_type = ResolvedTableRef::ObjectType::VIEW;

        const std::array<const char*, 7> col_names = {
            "object_id",
            "object_type",
            "schema_path",
            "full_path",
            "object_name",
            "dialect_tag",
            "compat_name"
        };

        ref.columns.clear();
        ref.columns.reserve(col_names.size());
        uint32_t index = 0;
        for (const auto* col_name : col_names)
        {
            ResolvedTableRef::ColumnInfo col_info;
            col_info.name = internString(col_name);
            col_info.data_type = DataType::VARCHAR;
            col_info.is_nullable = true;
            col_info.column_index = index++;
            ref.columns.push_back(col_info);
        }

        return ref;
    }

    auto make_monitor_ref =
        [&](std::string_view name,
            const std::vector<std::pair<const char*, DataType>>& cols) -> ResolvedTableRef {
        ResolvedTableRef ref;
        ref.table_uuid = ID{};
        ref.schema_uuid = ID{};
        ref.name = internString(name);
        ref.object_type = ResolvedTableRef::ObjectType::TABLE;

        ref.columns.clear();
        ref.columns.reserve(cols.size());
        uint32_t index = 0;
        for (const auto& col : cols)
        {
            ResolvedTableRef::ColumnInfo col_info;
            col_info.name = internString(col.first);
            col_info.data_type = col.second;
            col_info.is_nullable = true;
            col_info.column_index = index++;
            ref.columns.push_back(col_info);
        }

        return ref;
    };

    std::string table_name = components.back();
    std::string schema_path = components.size() > 1 ? join_components(components.size() - 1)
                                                    : std::string();
    auto resolve_virtual_table = [&](const std::string& schema_name,
                                     const std::string& base_name,
                                     const std::string& full_name) -> std::optional<ResolvedTableRef> {
        scratchbird::catalog::VirtualCatalogRouter& router =
            scratchbird::catalog::VirtualCatalogRouter::getInstance();
        if (!router.isInitialized())
        {
            return std::nullopt;
        }

        if (!scratchbird::catalog::isVirtualTable(schema_name, base_name))
        {
            return std::nullopt;
        }

        std::vector<core::CatalogManager::ColumnInfo> columns;
        core::ErrorContext err_ctx;
        auto status = router.getVirtualTableColumns(scratchbird::catalog::ProtocolType::SCRATCHBIRD,
                                                    schema_name,
                                                    base_name,
                                                    columns,
                                                    &err_ctx);
        if (status != Status::OK || columns.empty())
        {
            error(span, "Virtual table not found: " + full_name);
            return std::nullopt;
        }

        ResolvedTableRef ref;
        ref.table_uuid = ID{};
        ref.schema_uuid = ID{};
        ref.name = internString(full_name);
        ref.object_type = ResolvedTableRef::ObjectType::TABLE;
        ref.columns.reserve(columns.size());
        uint32_t index = 0;
        for (const auto& col : columns)
        {
            ResolvedTableRef::ColumnInfo col_info;
            col_info.name = internString(col.column_name);
            col_info.data_type = static_cast<DataType>(col.data_type);
            col_info.is_nullable = col.nullable;
            col_info.column_index = index++;
            ref.columns.push_back(col_info);
        }

        return ref;
    };

    if (path.type == PathType::UNQUALIFIED && components.size() == 1)
    {
        std::string upper_name = core::IdentifierUtils::toUpper(table_name);
        if (upper_name.rfind("MON_", 0) == 0)
        {
            if (upper_name == "MON_DATABASE")
            {
                return make_monitor_ref(
                    table_name,
                    {{"MON$DATABASE_NAME", DataType::VARCHAR},
                     {"MON$NEXT_TRANSACTION", DataType::INT64},
                     {"MON$OLDEST_TRANSACTION", DataType::INT64},
                     {"MON$OLDEST_ACTIVE", DataType::INT64},
                     {"MON$OLDEST_SNAPSHOT", DataType::INT64}});
            }
            if (upper_name == "MON_SWEEP")
            {
                return make_monitor_ref(
                    table_name,
                    {{"MON$SWEEP_COUNT", DataType::INT64},
                     {"MON$LAST_SWEEP_TIME", DataType::INT64},
                     {"MON$LAST_DURATION_MS", DataType::INT64},
                     {"MON$OIT_BEFORE", DataType::INT64},
                     {"MON$OIT_AFTER", DataType::INT64},
                     {"MON$TOTAL_SWEPT", DataType::INT64},
                     {"MON$IN_PROGRESS", DataType::BOOLEAN}});
            }
            if (upper_name == "MON_GARBAGE_COLLECTION")
            {
                return make_monitor_ref(
                    table_name,
                    {{"MON$TUPLES_REMOVED", DataType::INT64},
                     {"MON$PAGES_CLEANED", DataType::INT64},
                     {"MON$COOPERATIVE_RUNS", DataType::INT64},
                     {"MON$BACKGROUND_RUNS", DataType::INT64},
                     {"MON$LAST_BG_TIME", DataType::INT64},
                     {"MON$LAST_BG_DURATION_MS", DataType::INT64},
                     {"MON$DIRTY_PAGE_COUNT", DataType::INT64},
                     {"MON$SPACE_RECLAIMED", DataType::INT64},
                     {"MON$DURATION_0_10MS", DataType::INT64},
                     {"MON$DURATION_10_50MS", DataType::INT64},
                     {"MON$DURATION_50_100MS", DataType::INT64},
                     {"MON$DURATION_100_500MS", DataType::INT64},
                     {"MON$DURATION_500_1000MS", DataType::INT64},
                     {"MON$DURATION_1000MS_PLUS", DataType::INT64},
                     {"MON$PAGES_NO_GARBAGE", DataType::INT64},
                     {"MON$MAX_SPACE_RECLAIMED_PAGE", DataType::INT64},
                     {"MON$TOTAL_DIRTY_MARKED", DataType::INT64},
                     {"MON$COOPERATIVE_RATE", DataType::INT64},
                     {"MON$BACKGROUND_INTERVAL_MS", DataType::INT64}});
            }
            if (upper_name == "MON_ACTIVE_TRANSACTIONS")
            {
                return make_monitor_ref(
                    table_name,
                    {{"MON$TRANSACTION_ID", DataType::INT64},
                     {"MON$PROC_ID", DataType::INT64},
                     {"MON$AGE_SECONDS", DataType::INT64},
                     {"MON$ISOLATION_LEVEL", DataType::INT64},
                     {"MON$IS_READ_ONLY", DataType::BOOLEAN},
                     {"MON$START_TIME", DataType::INT64}});
            }

            error(span, "Unknown monitoring table: " + table_name);
            return std::nullopt;
        }

        if (upper_name == "DUAL")
        {
            return make_monitor_ref(
                table_name,
                {{"DUMMY", DataType::VARCHAR}});
        }
    }

    CatalogManager::TableInfo table_info;
    Status status = Status::NOT_FOUND;

    bool search_path_allowed = allow_search_path && !path.no_search_path;

    if (path.type == PathType::UNQUALIFIED)
    {
        if (components.size() != 1)
        {
            error(span, "Invalid table reference: too many parts in path");
            return std::nullopt;
        }

        if (search_path_allowed)
        {
            for (const auto& schema_id : search_path_)
            {
                status = catalog_.getTable(schema_id, table_name, table_info);
                if (status == Status::OK)
                {
                    break;
                }
            }
        }
        if (status != Status::OK && !isZeroUuidLocal(current_schema_))
        {
            status = catalog_.getTable(current_schema_, table_name, table_info);
        }
        if (status != Status::OK && !search_path_allowed && isZeroUuidLocal(current_schema_))
        {
            error(span, "Current schema not set");
            return std::nullopt;
        }
    }
    else
    {
        CatalogManager::SchemaInfo schema_info;
        ID schema_id{};
        bool schema_id_resolved = false;
        std::string resolved_schema_path = schema_path;

        if (path.type == PathType::CURRENT)
        {
            if (schema_path.empty())
            {
                schema_id = current_schema_;
                schema_id_resolved = true;
            }
            else if (!isZeroUuidLocal(current_schema_))
            {
                std::string current_path;
                core::ErrorContext err_ctx;
                if (catalog_.getSchemaPath(current_schema_, current_path, &err_ctx) == Status::OK &&
                    !current_path.empty())
                {
                    resolved_schema_path = current_path + "." + schema_path;
                }
            }
        }
        else if (path.type == PathType::PARENT)
        {
            if (isZeroUuidLocal(current_schema_))
            {
                error(span, "Current schema not set for parent resolution");
                return std::nullopt;
            }

            CatalogManager::SchemaInfo current_info;
            if (catalog_.getSchema(current_schema_, current_info) != Status::OK ||
                isZeroUuidLocal(current_info.parent_schema_id))
            {
                error(span, "Current schema has no parent");
                return std::nullopt;
            }

            if (schema_path.empty())
            {
                schema_id = current_info.parent_schema_id;
                schema_id_resolved = true;
            }
            else
            {
                std::string parent_path;
                core::ErrorContext err_ctx;
                if (catalog_.getSchemaPath(current_info.parent_schema_id, parent_path, &err_ctx) == Status::OK &&
                    !parent_path.empty())
                {
                    resolved_schema_path = parent_path + "." + schema_path;
                }
            }
        }
        else if (path.type == PathType::ABSOLUTE && schema_path.empty())
        {
            error(span, "Invalid table reference: missing schema");
            return std::nullopt;
        }

        if (!schema_id_resolved)
        {
            status = catalog_.getSchema(resolved_schema_path, schema_info);
            if (status != Status::OK)
            {
                if (auto virtual_ref = resolve_virtual_table(resolved_schema_path,
                                                            table_name,
                                                            resolved_schema_path + "." + table_name))
                {
                    return virtual_ref;
                }
                error(span, "Schema not found: " + resolved_schema_path);
                return std::nullopt;
            }
            schema_id = schema_info.schema_id;
        }

        status = catalog_.getTable(schema_id, table_name, table_info);
    }

    if (status != Status::OK)
    {
        error(span, "Table not found: " + table_name);
        return std::nullopt;
    }

    // Build resolved reference
    ResolvedTableRef ref;
    ref.table_uuid = table_info.table_id;
    ref.schema_uuid = table_info.schema_id;
    ref.name = internString(table_name);  // Preserve original name for bytecode emission

    // Check if this is actually a view, not a table
    CatalogManager::ViewInfo view_info;
    if (catalog_.getViewById(table_info.table_id, view_info) == Status::OK) {
        // It's a view
        if (view_info.materialized) {
            ref.object_type = ResolvedTableRef::ObjectType::MATERIALIZED_VIEW;
        } else {
            ref.object_type = ResolvedTableRef::ObjectType::VIEW;
        }
    } else {
        // It's a real table
        ref.object_type = ResolvedTableRef::ObjectType::TABLE;
    }

    // Load columns
    loadTableColumns(ref);

    return ref;
}

bool SemanticAnalyzerV2::loadTableColumns(ResolvedTableRef& ref) {
    std::vector<CatalogManager::ColumnInfo> columns;
    Status status = catalog_.getColumns(ref.table_uuid, columns);

    if (status != Status::OK) {
        return false;
    }

    ref.columns.clear();
    ref.columns.reserve(columns.size());

    for (size_t i = 0; i < columns.size(); ++i) {
        ResolvedTableRef::ColumnInfo col_info;
        col_info.name = internString(columns[i].column_name);
        col_info.data_type = static_cast<DataType>(columns[i].data_type);
        col_info.is_nullable = columns[i].nullable;
        col_info.column_index = static_cast<uint32_t>(i);
        ref.columns.push_back(col_info);
    }

    return true;
}

std::optional<ResolvedColumnRef> SemanticAnalyzerV2::resolveColumn(
    StringPool::StringId table_alias,
    StringPool::StringId column_name,
    SourceSpan span)
{
    if (scope_stack_.empty()) {
        error(span, "No tables in scope for column resolution");
        return std::nullopt;
    }

    auto normalize_name = [&](StringPool::StringId name_id) {
        return core::IdentifierUtils::toUpper(std::string(getString(name_id)));
    };

    if (table_alias != StringPool::INVALID_ID) {
        // Qualified column reference
        const auto* col = currentScope().findColumn(table_alias, column_name);
        const auto* table = currentScope().findTable(table_alias);
        if (!col && table && isZeroUuidLocal(table->table_uuid)) {
            std::string target = normalize_name(column_name);
            for (const auto& candidate : table->columns) {
                if (normalize_name(candidate.name) == target) {
                    col = &candidate;
                    break;
                }
            }
        }
        if (!col) {
            std::string col_str = std::string(getString(column_name));
            std::string table_str = std::string(getString(table_alias));
            error(span, "Column not found: " + table_str + "." + col_str);
            return std::nullopt;
        }

        ResolvedColumnRef ref;
        ref.table_uuid = table->table_uuid;
        ref.column_index = col->column_index;
        ref.data_type = col->data_type;
        ref.is_nullable = col->is_nullable;
        ref.column_name = column_name;
        ref.table_alias = table_alias;
        return ref;
    }

    // Unqualified - search all tables
    auto result = currentScope().findColumn(column_name);
    if (!result.column) {
        std::string target = normalize_name(column_name);
        const ResolutionScope::TableEntry* match_table = nullptr;
        const ResolvedTableRef::ColumnInfo* match_col = nullptr;

        for (const auto& table : currentScope().tables()) {
            if (!isZeroUuidLocal(table.table_uuid)) {
                continue;
            }
            for (const auto& candidate : table.columns) {
                if (normalize_name(candidate.name) == target) {
                    if (match_col != nullptr) {
                        result.ambiguous = true;
                        break;
                    }
                    match_table = &table;
                    match_col = &candidate;
                }
            }
            if (result.ambiguous) {
                break;
            }
        }

        if (match_col) {
            result.table = match_table;
            result.column = match_col;
        }
    }
    if (result.ambiguous) {
        error(span, "Ambiguous column reference: " + std::string(getString(column_name)));
        return std::nullopt;
    }
    if (!result.column) {
        error(span, "Column not found: " + std::string(getString(column_name)));
        return std::nullopt;
    }

    ResolvedColumnRef ref;
    ref.table_uuid = result.table->table_uuid;
    ref.column_index = result.column->column_index;
    ref.data_type = result.column->data_type;
    ref.is_nullable = result.column->is_nullable;
    ref.column_name = column_name;
    ref.table_alias = result.table->alias;
    return ref;
}

std::optional<ResolvedFunctionRef> SemanticAnalyzerV2::resolveFunction(
    const SchemaPath& path,
    const std::vector<ResolvedType>& arg_types,
    SourceSpan span)
{
    if (path.components.empty()) {
        error(span, "Empty function name");
        return std::nullopt;
    }

    std::string func_name;
    if (path.components.size() == 1) {
        func_name = std::string(string_pool_.get(path.components[0]));
    } else {
        func_name = std::string(string_pool_.get(path.components.back()));
        // Try package dependency on prefix
        std::string pkg_name = std::string(string_pool_.get(path.components.front()));
        core::CatalogManager::PackageInfo pkg;
        core::ErrorContext ctx;
        if (!isZeroUuidLocal(current_schema_) &&
            catalog_.getPackageByName(current_schema_, pkg_name, pkg, &ctx) == Status::OK) {
            if (current_result_) {
                current_result_->addDependency(pkg.package_id, core::CatalogManager::ObjectType::PACKAGE);
            }
        }
    }

    ResolvedFunctionRef ref;
    ref.function_uuid = ID{};  // Zero UUID for built-in
    ref.function_name = path.components.back();
    ref.is_builtin = true;
    ref.kind = FunctionKind::BUILTIN;

    // Create return type in arena
    auto* ret_type = arena_.create<ResolvedType>();

    // Determine function type and return type based on name
    // Spec: docs/specifications/INTERNAL_FUNCTIONS.md
    std::transform(func_name.begin(), func_name.end(), func_name.begin(), ::tolower);

    // Aggregate functions
    if (func_name == "count" || func_name == "sum" || func_name == "avg" ||
        func_name == "min" || func_name == "max" ||
        func_name == "stddev" || func_name == "stddev_samp" || func_name == "stddev_pop" ||
        func_name == "variance" || func_name == "var_samp" || func_name == "var_pop" ||
        func_name == "corr" || func_name == "covar_pop" || func_name == "array_agg") {
        ref.is_aggregate = true;

        if (func_name == "array_agg") {
            ret_type->data_type = DataType::JSON;
        } else if (func_name == "count") {
            ret_type->data_type = DataType::INT64;
        } else if (func_name == "avg" || func_name == "stddev" ||
                   func_name == "stddev_samp" || func_name == "stddev_pop" ||
                   func_name == "variance" || func_name == "var_samp" ||
                   func_name == "var_pop" || func_name == "corr" ||
                   func_name == "covar_pop") {
            ret_type->data_type = DataType::FLOAT64;
        } else if (!arg_types.empty()) {
            *ret_type = arg_types[0];
        }
        ret_type->is_nullable = false;
        ref.return_type = ret_type;
        return ref;
    }

    // String functions
    if (func_name == "length" || func_name == "char_length" || func_name == "octet_length") {
        ret_type->data_type = DataType::INT32;
        ret_type->is_nullable = !arg_types.empty() && arg_types[0].is_nullable;
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "upper" || func_name == "lower" || func_name == "trim" ||
        func_name == "ltrim" || func_name == "rtrim" || func_name == "substring") {
        ret_type->data_type = DataType::VARCHAR;
        ret_type->is_nullable = !arg_types.empty() && arg_types[0].is_nullable;
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "concat" || func_name == "concat_ws") {
        ret_type->data_type = DataType::VARCHAR;
        ret_type->is_nullable = true;
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "format_type" || func_name == "obj_description" ||
        func_name == "col_description" || func_name == "shobj_description") {
        ret_type->data_type = DataType::TEXT;
        ret_type->is_nullable = true;
        ref.return_type = ret_type;
        return ref;
    }

    // Date/time functions
    if (func_name == "now" || func_name == "current_timestamp") {
        ret_type->data_type = DataType::TIMESTAMP;
        ret_type->with_time_zone = true;
        ret_type->is_nullable = false;
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "current_date") {
        ret_type->data_type = DataType::DATE;
        ret_type->is_nullable = false;
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "current_time") {
        ret_type->data_type = DataType::TIME;
        ret_type->with_time_zone = true;
        ret_type->is_nullable = false;
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "date_add" || func_name == "date_sub") {
        if (!arg_types.empty()) {
            *ret_type = arg_types[0];
        } else {
            ret_type->data_type = DataType::TIMESTAMP;
        }
        ret_type->is_nullable = true;
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "date_diff" || func_name == "datediff") {
        ret_type->data_type = DataType::INT64;
        ret_type->is_nullable = true;
        ref.return_type = ret_type;
        return ref;
    }

    // Spatial functions
    if (func_name == "st_point") {
        ret_type->data_type = DataType::POINT;
        ret_type->is_nullable = true;
        ref.return_type = ret_type;
        return ref;
    }
    if (func_name == "st_makeline") {
        ret_type->data_type = DataType::LINESTRING;
        ret_type->is_nullable = true;
        ref.return_type = ret_type;
        return ref;
    }
    if (func_name == "st_makepolygon") {
        ret_type->data_type = DataType::POLYGON;
        ret_type->is_nullable = true;
        ref.return_type = ret_type;
        return ref;
    }
    if (func_name == "st_astext" || func_name == "st_asbinary" ||
        func_name == "st_geometrytype") {
        ret_type->data_type = DataType::TEXT;
        ret_type->is_nullable = true;
        ref.return_type = ret_type;
        return ref;
    }
    if (func_name == "st_isvalid") {
        ret_type->data_type = DataType::BOOLEAN;
        ret_type->is_nullable = true;
        ref.return_type = ret_type;
        return ref;
    }

    // Numeric functions
    if (func_name == "abs") {
        if (!arg_types.empty()) {
            *ret_type = arg_types[0];
        } else {
            ret_type->data_type = DataType::FLOAT64;
        }
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "sign") {
        ret_type->data_type = DataType::INT32;
        ret_type->is_nullable = !arg_types.empty() && arg_types[0].is_nullable;
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "round" || func_name == "ceil" || func_name == "floor" ||
        func_name == "trunc" || func_name == "mod") {
        ret_type->data_type = DataType::FLOAT64;
        ret_type->is_nullable = !arg_types.empty() && arg_types[0].is_nullable;
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "sqrt" || func_name == "log" || func_name == "ln" ||
        func_name == "exp" || func_name == "power" || func_name == "pow" ||
        func_name == "sin" || func_name == "cos" || func_name == "tan" ||
        func_name == "asin" || func_name == "acos" || func_name == "atan" ||
        func_name == "atan2" || func_name == "degrees" || func_name == "radians" ||
        func_name == "pi" || func_name == "sinh" || func_name == "cosh" ||
        func_name == "tanh" || func_name == "asinh" || func_name == "acosh" ||
        func_name == "atanh" || func_name == "cot" || func_name == "cbrt" ||
        func_name == "log10" || func_name == "log2") {
        ret_type->data_type = DataType::FLOAT64;
        ret_type->is_nullable = !arg_types.empty() && arg_types[0].is_nullable;
        if (func_name == "pi") {
            ret_type->is_nullable = false;
        }
        ref.return_type = ret_type;
        return ref;
    }

    // JSON functions
    if (func_name == "json_extract" || func_name == "json_set" ||
        func_name == "json_insert" || func_name == "json_remove" ||
        func_name == "json_object" || func_name == "json_array") {
        ret_type->data_type = DataType::JSON;
        ret_type->is_nullable = !arg_types.empty() && arg_types[0].is_nullable;
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "jsonb_extract_path" || func_name == "jsonb_build_object" ||
        func_name == "jsonb_build_array" || func_name == "jsonb_set") {
        ret_type->data_type = DataType::JSONB;
        ret_type->is_nullable = !arg_types.empty() && arg_types[0].is_nullable;
        ref.return_type = ret_type;
        return ref;
    }

    // Coalesce/nullif
    if (func_name == "coalesce") {
        if (arg_types.empty()) {
            error(span, "COALESCE requires at least one argument");
            return std::nullopt;
        }
        *ret_type = arg_types[0];
        ret_type->is_nullable = true;
        for (const auto& arg : arg_types) {
            ret_type->is_nullable = ret_type->is_nullable || arg.is_nullable;
        }
        ref.return_type = ret_type;
        return ref;
    }

    if (func_name == "nullif") {
        if (arg_types.size() != 2) {
            error(span, "NULLIF requires exactly two arguments");
            return std::nullopt;
        }
        *ret_type = arg_types[0];
        ret_type->is_nullable = true;
        ref.return_type = ret_type;
        return ref;
    }

    // Not a built-in: try catalog-resolved function/procedure/UDR
    core::ErrorContext ctx;
    core::CatalogManager::FunctionInfo fi;
    if (catalog_.getFunction(func_name, fi, &ctx) == Status::OK) {
        ref.function_uuid = fi.function_id;
        ref.is_builtin = false;
        ref.kind = FunctionKind::FUNCTION;
        ret_type->data_type = fi.return_type;
        ret_type->precision = static_cast<int32_t>(fi.return_type_precision);
        ret_type->scale = static_cast<int32_t>(fi.return_type_scale);
        ret_type->is_nullable = true;
        ref.return_type = ret_type;
        if (current_result_) {
            current_result_->addDependency(fi.function_id, core::CatalogManager::ObjectType::FUNCTION);
        }
        return ref;
    }

    core::CatalogManager::ProcedureInfo pi;
    if (catalog_.getProcedure(func_name, pi, &ctx) == Status::OK) {
        error(span, "Procedures cannot be used in expressions: " + func_name);
        return std::nullopt;
    }

    core::CatalogManager::UDRInfo ui;
    if (catalog_.getUDRByName(current_schema_, func_name, ui, &ctx) == Status::OK) {
        if (ui.udr_type != core::CatalogManager::UDRType::FUNCTION) {
            error(span, "UDR is not a function: " + func_name);
            return std::nullopt;
        }
        ref.function_uuid = ui.udr_id;
        ref.is_builtin = false;
        ref.kind = FunctionKind::UDR;
        ret_type->data_type = DataType::UNKNOWN;
        ret_type->is_nullable = true;
        ref.return_type = ret_type;
        if (current_result_) {
            current_result_->addDependency(ui.udr_id, core::CatalogManager::ObjectType::UDR);
        }
        return ref;
    }

    error(span, "Unknown function: " + func_name);
    return std::nullopt;
}

// =============================================================================
// Type Checking
// =============================================================================

std::optional<ResolvedType> SemanticAnalyzerV2::getCommonType(
    const ResolvedType& left,
    const ResolvedType& right,
    BinaryOp op)
{
    if (op == BinaryOp::REGEX_MATCH || op == BinaryOp::REGEX_MATCH_CI ||
        op == BinaryOp::REGEX_NOT_MATCH || op == BinaryOp::REGEX_NOT_MATCH_CI) {
        ResolvedType result;
        result.data_type = DataType::BOOLEAN;
        result.is_nullable = left.is_nullable || right.is_nullable ||
                             left.data_type == DataType::UNKNOWN ||
                             right.data_type == DataType::UNKNOWN;
        auto is_text = [](const ResolvedType& t) {
            return t.isString() || t.data_type == DataType::UNKNOWN;
        };
        if (is_text(left) && is_text(right)) {
            return result;
        }
        return std::nullopt;
    }

    // Comparison operators always yield boolean
    if (op == BinaryOp::EQ || op == BinaryOp::NE || op == BinaryOp::LT ||
        op == BinaryOp::LE || op == BinaryOp::GT || op == BinaryOp::GE) {
        ResolvedType result;
        result.data_type = DataType::BOOLEAN;
        result.is_nullable = left.is_nullable || right.is_nullable ||
                             left.data_type == DataType::UNKNOWN ||
                             right.data_type == DataType::UNKNOWN;
        if (left.data_type == DataType::UNKNOWN || right.data_type == DataType::UNKNOWN) {
            return result;
        }
        if (left.isComparableTo(right)) {
            return result;
        }
        return std::nullopt;
    }

    // Logical operators require boolean operands and yield boolean
    if (op == BinaryOp::AND || op == BinaryOp::OR) {
        if (left.isBoolean() && right.isBoolean()) {
            ResolvedType result;
            result.data_type = DataType::BOOLEAN;
            result.is_nullable = left.is_nullable || right.is_nullable;
            return result;
        }
        return std::nullopt;
    }

    // Handle NULL (UNKNOWN) type - NULL adopts the type of the other operand
    // For any operation with NULL, the result type comes from the non-NULL side
    if (left.data_type == DataType::UNKNOWN && right.data_type != DataType::UNKNOWN) {
        ResolvedType result = right;
        result.is_nullable = true;  // NULL makes result nullable
        return result;
    }
    if (right.data_type == DataType::UNKNOWN && left.data_type != DataType::UNKNOWN) {
        ResolvedType result = left;
        result.is_nullable = true;  // NULL makes result nullable
        return result;
    }
    if (left.data_type == DataType::UNKNOWN && right.data_type == DataType::UNKNOWN) {
        // Both are NULL - result is NULL with UNKNOWN type
        ResolvedType result;
        result.data_type = DataType::UNKNOWN;
        result.is_nullable = true;
        return result;
    }

    // Same type - use it
    if (left.data_type == right.data_type) {
        ResolvedType result = left;
        result.is_nullable = left.is_nullable || right.is_nullable;
        return result;
    }

    // Comparison operators result in boolean
    switch (op) {
        case BinaryOp::ADD:
        case BinaryOp::SUB:
        case BinaryOp::MUL:
        case BinaryOp::DIV:
        case BinaryOp::MOD:
            if (left.isNumeric() && right.isNumeric()) {
                ResolvedType result;
                result.is_nullable = left.is_nullable || right.is_nullable;

                if (left.data_type == DataType::FLOAT64 || right.data_type == DataType::FLOAT64) {
                    result.data_type = DataType::FLOAT64;
                } else if (left.data_type == DataType::FLOAT32 || right.data_type == DataType::FLOAT32) {
                    result.data_type = DataType::FLOAT32;
                } else if (left.data_type == DataType::DECIMAL || right.data_type == DataType::DECIMAL) {
                    result.data_type = DataType::DECIMAL;
                } else if (left.data_type == DataType::INT64 || right.data_type == DataType::INT64) {
                    result.data_type = DataType::INT64;
                } else if (left.data_type == DataType::INT32 || right.data_type == DataType::INT32) {
                    result.data_type = DataType::INT32;
                } else {
                    result.data_type = DataType::INT16;
                }
                return result;
            }
            return std::nullopt;

        case BinaryOp::CONCAT:
            if (left.isString() && right.isString()) {
                ResolvedType result;
                result.data_type = DataType::VARCHAR;
                result.is_nullable = left.is_nullable || right.is_nullable;
                return result;
            }
            return std::nullopt;

        case BinaryOp::JSON_EXTRACT:
        case BinaryOp::JSON_EXTRACT_TEXT:
        case BinaryOp::JSON_HASH_EXTRACT:
        case BinaryOp::JSON_HASH_EXTRACT_TEXT: {
            auto is_json_like = [](const ResolvedType& t) {
                return t.isString() || t.data_type == DataType::JSON ||
                       t.data_type == DataType::JSONB || t.data_type == DataType::UNKNOWN;
            };
            auto is_path_text = [](const ResolvedType& t) {
                return t.isString() || t.data_type == DataType::UNKNOWN;
            };

            if (!is_json_like(left)) {
                return std::nullopt;
            }

            if (op == BinaryOp::JSON_HASH_EXTRACT || op == BinaryOp::JSON_HASH_EXTRACT_TEXT) {
                if (!right.is_array && right.data_type != DataType::UNKNOWN) {
                    return std::nullopt;
                }
            } else {
                if (!is_path_text(right)) {
                    return std::nullopt;
                }
            }

            ResolvedType result;
            DataType json_type = (left.data_type == DataType::JSONB) ? DataType::JSONB
                                                                     : DataType::JSON;
            result.data_type = (op == BinaryOp::JSON_EXTRACT_TEXT ||
                                op == BinaryOp::JSON_HASH_EXTRACT_TEXT)
                                   ? DataType::TEXT
                                   : json_type;
            result.is_nullable = left.is_nullable || right.is_nullable;
            return result;
        }

        default:
            break;
    }

    return std::nullopt;
}

bool SemanticAnalyzerV2::canImplicitCast(const ResolvedType& from, const ResolvedType& to) {
    if (from.data_type == to.data_type) {
        return true;
    }
    if (from.isNumeric() && to.isNumeric()) {
        return true;
    }
    if (from.isString() && to.isString()) {
        return true;
    }
    return false;
}

ResolvedExpression* SemanticAnalyzerV2::insertImplicitCast(
    ResolvedExpression* expr,
    const ResolvedType& target_type)
{
    if (expr->type.data_type == target_type.data_type) {
        return expr;
    }

    auto* cast = arena_.create<ResolvedCast>();
    cast->expr = expr;
    cast->target_type = target_type;
    cast->type = target_type;
    cast->format = core::CastFormat::DEFAULT;
    cast->implicit = true;
    cast->span = expr->span;
    return cast;
}

// =============================================================================
// Statement Analysis
// =============================================================================

ResolvedStatement* SemanticAnalyzerV2::analyzeStatement(Statement* stmt) {
    if (!stmt) {
        return nullptr;
    }

    switch (stmt->kind()) {
        // DDL
        case ASTKind::CreateTableStmt:
            return analyzeCreateTable(static_cast<CreateTableStmt*>(stmt));
        case ASTKind::CreateIndexStmt:
            return analyzeCreateIndex(static_cast<CreateIndexStmt*>(stmt));
        case ASTKind::CreateViewStmt:
            return analyzeCreateView(static_cast<CreateViewStmt*>(stmt));
        case ASTKind::CreateSchemaStmt:
            return analyzeCreateSchema(static_cast<CreateSchemaStmt*>(stmt));
        case ASTKind::DropSchemaStmt:
            return analyzeDropSchema(static_cast<DropSchemaStmt*>(stmt));
        case ASTKind::AlterSchemaStmt:
            return analyzeAlterSchema(static_cast<AlterSchemaStmt*>(stmt));
        case ASTKind::CreateDatabaseStmt:
            return analyzeCreateDatabase(static_cast<CreateDatabaseStmt*>(stmt));
        case ASTKind::CreateFunctionStmt:
            return analyzeCreateFunction(static_cast<CreateFunctionStmt*>(stmt));
        case ASTKind::CreateProcedureStmt:
            return analyzeCreateProcedure(static_cast<CreateProcedureStmt*>(stmt));
        case ASTKind::CreateTriggerStmt:
            return analyzeCreateTrigger(static_cast<CreateTriggerStmt*>(stmt));
        case ASTKind::CreatePackageStmt:
            return analyzeCreatePackage(static_cast<CreatePackageStmt*>(stmt));
        case ASTKind::CreateRoleStmt:
            return analyzeCreateRole(static_cast<CreateRoleStmt*>(stmt));
        case ASTKind::CreateExceptionStmt:
            return analyzeCreateException(static_cast<CreateExceptionStmt*>(stmt));
        case ASTKind::CreateDomainStmt:
            return analyzeCreateDomain(static_cast<CreateDomainStmt*>(stmt));
        case ASTKind::AlterDomainStmt:
            return analyzeAlterDomain(static_cast<AlterDomainStmt*>(stmt));
        case ASTKind::DropDomainStmt:
            return analyzeDropDomain(static_cast<DropDomainStmt*>(stmt));
        case ASTKind::DropDatabaseStmt:
            return analyzeDropDatabase(static_cast<DropDatabaseStmt*>(stmt));
        case ASTKind::AlterDatabaseStmt:
            return analyzeAlterDatabase(static_cast<AlterDatabaseStmt*>(stmt));
        case ASTKind::AlterTableStmt:
            return analyzeAlterTable(static_cast<AlterTableStmt*>(stmt));
        case ASTKind::AlterIndexStmt:
            return analyzeAlterIndex(static_cast<AlterIndexStmt*>(stmt));
        case ASTKind::RenameObjectStmt:
            return analyzeRenameObject(static_cast<RenameObjectStmt*>(stmt));
        case ASTKind::MoveObjectStmt:
            return analyzeMoveObject(static_cast<MoveObjectStmt*>(stmt));
        case ASTKind::DropTableStmt:
            return analyzeDropTable(static_cast<DropTableStmt*>(stmt));
        case ASTKind::DropIndexStmt:
            return analyzeDropIndex(static_cast<DropIndexStmt*>(stmt));
        case ASTKind::DropViewStmt:
            return analyzeDropView(static_cast<DropViewStmt*>(stmt));
        case ASTKind::DropSequenceStmt:
            return analyzeDropSequence(static_cast<DropSequenceStmt*>(stmt));
        case ASTKind::DropFunctionStmt:
            return analyzeDropFunction(static_cast<DropFunctionStmt*>(stmt));
        case ASTKind::DropProcedureStmt:
            return analyzeDropProcedure(static_cast<DropProcedureStmt*>(stmt));
        case ASTKind::DropTriggerStmt:
            return analyzeDropTrigger(static_cast<DropTriggerStmt*>(stmt));
        case ASTKind::DropPackageStmt:
            return analyzeDropPackage(static_cast<DropPackageStmt*>(stmt));
        case ASTKind::DropRoleStmt:
            return analyzeDropRole(static_cast<DropRoleStmt*>(stmt));
        case ASTKind::DropExceptionStmt:
            return analyzeDropException(static_cast<DropExceptionStmt*>(stmt));
        case ASTKind::TruncateTableStmt:
            return analyzeTruncateTable(static_cast<TruncateTableStmt*>(stmt));

        // DML
        case ASTKind::SelectStmt:
            return analyzeSelect(static_cast<SelectStmt*>(stmt));
        case ASTKind::InsertStmt:
            return analyzeInsert(static_cast<InsertStmt*>(stmt));
        case ASTKind::UpdateStmt:
            return analyzeUpdate(static_cast<UpdateStmt*>(stmt));
        case ASTKind::DeleteStmt:
            return analyzeDelete(static_cast<DeleteStmt*>(stmt));
        case ASTKind::CopyStmt:
            return analyzeCopy(static_cast<CopyStmt*>(stmt));

        // Transaction
        case ASTKind::StartTransactionStmt:
            return analyzeStartTransaction(static_cast<StartTransactionStmt*>(stmt));
        case ASTKind::PrepareTransactionStmt:
            return analyzePrepareTransaction(static_cast<PrepareTransactionStmt*>(stmt));
        case ASTKind::CommitStmt:
            return analyzeCommit(static_cast<CommitStmt*>(stmt));
        case ASTKind::RollbackStmt:
            return analyzeRollback(static_cast<RollbackStmt*>(stmt));
        case ASTKind::SavepointStmt:
            return analyzeSavepoint(static_cast<SavepointStmt*>(stmt));
        case ASTKind::ReleaseSavepointStmt:
            return analyzeReleaseSavepoint(static_cast<ReleaseSavepointStmt*>(stmt));

        // Session
        case ASTKind::SetStmt:
            return analyzeSet(static_cast<SetStmt*>(stmt));
        case ASTKind::ShowStmt:
            return analyzeShow(static_cast<ShowStmt*>(stmt));
        case ASTKind::ExplainStmt:
            return analyzeExplain(static_cast<ExplainStmt*>(stmt));

        // DCL
        case ASTKind::GrantStmt:
            return analyzeGrant(static_cast<GrantStmt*>(stmt));
        case ASTKind::RevokeStmt:
            return analyzeRevoke(static_cast<RevokeStmt*>(stmt));

        default:
            error(stmt->span, "Unsupported statement type for semantic analysis");
            return nullptr;
    }
}

// =============================================================================
// Transaction/Session Statement Analysis
// =============================================================================

ResolvedStatement* SemanticAnalyzerV2::analyzeStartTransaction(StartTransactionStmt* stmt) {
    auto* resolved = arena_.create<ResolvedStartTransactionStmt>();
    resolved->span = stmt->span;
    resolved->has_isolation_level = stmt->has_isolation_level;
    resolved->isolation_level = stmt->isolation_level;
    resolved->has_access_mode = stmt->has_access_mode;
    resolved->access_mode = stmt->access_mode;
    resolved->has_read_committed_mode = stmt->has_read_committed_mode;
    resolved->read_committed_mode = stmt->read_committed_mode;
    resolved->has_deferrable = stmt->deferrable || stmt->not_deferrable;
    resolved->deferrable = stmt->deferrable;
    resolved->has_wait_mode = stmt->has_wait_mode;
    resolved->wait_mode = stmt->wait_mode;
    resolved->has_lock_timeout = stmt->has_lock_timeout;
    resolved->lock_timeout_seconds = stmt->lock_timeout_seconds;
    resolved->table_reservations = stmt->table_reservations;
    resolved->has_autocommit = stmt->has_autocommit;
    resolved->autocommit_mode = stmt->autocommit_mode;
    resolved->conflict_action = stmt->conflict_action;
    resolved->has_conflict_error_code = stmt->has_conflict_error_code;
    resolved->conflict_error_code = stmt->conflict_error_code;
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzePrepareTransaction(PrepareTransactionStmt* stmt) {
    auto* resolved = arena_.create<ResolvedPrepareTransactionStmt>();
    resolved->span = stmt->span;
    resolved->gid = stmt->gid;
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCommit(CommitStmt* stmt) {
    auto* resolved = arena_.create<ResolvedCommitStmt>();
    resolved->span = stmt->span;
    resolved->and_chain = stmt->and_chain;
    resolved->and_no_chain = stmt->and_no_chain;
    resolved->retaining = stmt->retaining;
    resolved->is_prepared = stmt->is_prepared;
    resolved->prepared_gid = stmt->prepared_gid;
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeRollback(RollbackStmt* stmt) {
    auto* resolved = arena_.create<ResolvedRollbackStmt>();
    resolved->span = stmt->span;
    resolved->to_savepoint = stmt->to_savepoint;
    resolved->savepoint_name = stmt->savepoint_name;
    resolved->and_chain = stmt->and_chain;
    resolved->and_no_chain = stmt->and_no_chain;
    resolved->retaining = stmt->retaining;
    resolved->is_prepared = stmt->is_prepared;
    resolved->prepared_gid = stmt->prepared_gid;
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeSavepoint(SavepointStmt* stmt) {
    auto* resolved = arena_.create<ResolvedSavepointStmt>();
    resolved->span = stmt->span;
    resolved->name = stmt->name;
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeReleaseSavepoint(ReleaseSavepointStmt* stmt) {
    auto* resolved = arena_.create<ResolvedSavepointStmt>();
    resolved->span = stmt->span;
    resolved->name = stmt->name;
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeSet(SetStmt* stmt) {
    auto* resolved = arena_.create<ResolvedSetStmt>();
    resolved->span = stmt->span;
    resolved->set_type = stmt->set_type;
    resolved->scope = stmt->scope;
    resolved->variable_name = stmt->name;
    resolved->is_default = stmt->is_default;

    if (stmt->value) {
        resolved->value = analyzeExpression(stmt->value);
    }

    resolved->has_isolation_level = stmt->has_isolation_level;
    resolved->isolation_level = stmt->isolation_level;
    resolved->has_access_mode = stmt->has_access_mode;
    resolved->access_mode = stmt->access_mode;
    resolved->has_read_committed_mode = stmt->has_read_committed_mode;
    resolved->read_committed_mode = stmt->read_committed_mode;
    resolved->has_deferrable = stmt->deferrable || stmt->not_deferrable;
    resolved->deferrable = stmt->deferrable;
    resolved->has_wait_mode = stmt->has_wait_mode;
    resolved->wait_mode = stmt->wait_mode;
    resolved->has_lock_timeout = stmt->has_lock_timeout;
    resolved->lock_timeout_seconds = stmt->lock_timeout_seconds;
    resolved->table_reservations = stmt->table_reservations;
    resolved->has_autocommit = stmt->has_autocommit;
    resolved->autocommit_mode = stmt->autocommit_mode;
    resolved->conflict_action = stmt->conflict_action;
    resolved->has_conflict_error_code = stmt->has_conflict_error_code;
    resolved->conflict_error_code = stmt->conflict_error_code;

    resolved->sql_dialect = stmt->sql_dialect;
    resolved->local_timeout_seconds = stmt->local_timeout_seconds;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeShow(ShowStmt* stmt) {
    auto* resolved = arena_.create<ResolvedShowStmt>();
    resolved->span = stmt->span;
    resolved->show_type = stmt->show_type;
    resolved->variable_name = stmt->name;
    resolved->from_name = stmt->from_name;
    resolved->like_pattern = stmt->like_pattern;
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeGrant(GrantStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedGrantStmt>();
    resolved->span = stmt->span;
    resolved->object_type = stmt->object_type;
    resolved->objects = stmt->objects;
    resolved->grantees = stmt->grantees;
    resolved->with_grant_option = stmt->with_grant_option;
    resolved->is_public = stmt->is_public;

    uint32_t mask = 0;
    for (auto priv : stmt->privileges) {
        switch (priv) {
            case PrivilegeType::SELECT:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::SELECT);
                break;
            case PrivilegeType::INSERT:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::INSERT);
                break;
            case PrivilegeType::UPDATE:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::UPDATE);
                break;
            case PrivilegeType::DELETE:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::DELETE);
                break;
            case PrivilegeType::TRUNCATE:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::TRUNCATE);
                break;
            case PrivilegeType::REFERENCES:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::REFERENCES);
                break;
            case PrivilegeType::TRIGGER:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::TRIGGER);
                break;
            case PrivilegeType::EXECUTE:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::EXECUTE);
                break;
            case PrivilegeType::USAGE:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::USAGE);
                break;
            case PrivilegeType::COPY:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::COPY_FILE);
                break;
            case PrivilegeType::ALL:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::ALL);
                break;
        }
    }
    resolved->privileges = mask;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeRevoke(RevokeStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedRevokeStmt>();
    resolved->span = stmt->span;
    resolved->object_type = stmt->object_type;
    resolved->objects = stmt->objects;
    resolved->grantees = stmt->grantees;
    resolved->grant_option_for = stmt->grant_option_for;
    resolved->cascade = stmt->cascade;
    resolved->is_public = stmt->is_public;

    uint32_t mask = 0;
    for (auto priv : stmt->privileges) {
        switch (priv) {
            case PrivilegeType::SELECT:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::SELECT);
                break;
            case PrivilegeType::INSERT:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::INSERT);
                break;
            case PrivilegeType::UPDATE:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::UPDATE);
                break;
            case PrivilegeType::DELETE:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::DELETE);
                break;
            case PrivilegeType::TRUNCATE:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::TRUNCATE);
                break;
            case PrivilegeType::REFERENCES:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::REFERENCES);
                break;
            case PrivilegeType::TRIGGER:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::TRIGGER);
                break;
            case PrivilegeType::EXECUTE:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::EXECUTE);
                break;
            case PrivilegeType::USAGE:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::USAGE);
                break;
            case PrivilegeType::COPY:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::COPY_FILE);
                break;
            case PrivilegeType::ALL:
                mask |= static_cast<uint32_t>(core::CatalogManager::Privilege::ALL);
                break;
        }
    }
    resolved->privileges = mask;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeExplain(ExplainStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedExplainStmt>();
    resolved->span = stmt->span;
    resolved->analyze = stmt->analyze;
    resolved->verbose = stmt->verbose;
    resolved->costs = stmt->costs;
    resolved->buffers = stmt->buffers;
    resolved->timing = stmt->timing;
    resolved->format_json = stmt->format_json;
    resolved->format_xml = stmt->format_xml;
    resolved->format_yaml = stmt->format_yaml;

    // Analyze the query to explain
    if (stmt->query) {
        resolved->query = analyzeStatement(stmt->query);
        if (!resolved->query) {
            // Query analysis failed - error already reported
            return nullptr;
        }
    } else {
        error(stmt->span, "EXPLAIN requires a query to explain");
        return nullptr;
    }

    return resolved;
}

// =============================================================================
// DDL Statement Analysis
// =============================================================================

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateTable(CreateTableStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateTableStmt>();
    resolved->span = stmt->span;
    resolved->if_not_exists = stmt->if_not_exists;

    // Resolve schema from table path
    if (stmt->table_path.components.size() >= 2) {
        std::string schema_name = std::string(string_pool_.get(stmt->table_path.components[0]));
        CatalogManager::SchemaInfo schema_info;
        if (catalog_.getSchema(schema_name, schema_info) == Status::OK) {
            resolved->schema.schema_uuid = schema_info.schema_id;
            resolved->schema.schema_name = stmt->table_path.components[0];
        }
        resolved->table_name = stmt->table_path.components[1];
    } else if (stmt->table_path.components.size() == 1) {
        // Use current schema
        resolved->schema.schema_uuid = current_schema_;
        resolved->table_name = stmt->table_path.components[0];
    }

    // Analyze column definitions
    for (auto* col_def : stmt->columns) {
        ResolvedColumnDef resolved_col = analyzeColumnDef(col_def);
        resolved->columns.push_back(resolved_col);
    }

    // Analyze table constraints
    for (auto* constraint : stmt->constraints) {
        ResolvedTableConstraint resolved_constraint = analyzeTableConstraint(constraint, resolved->columns);
        resolved->constraints.push_back(resolved_constraint);
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateIndex(CreateIndexStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateIndexStmt>();
    resolved->span = stmt->span;
    resolved->index_name = stmt->index_name;
    resolved->unique = stmt->unique;
    resolved->if_not_exists = stmt->if_not_exists;
    resolved->concurrent = stmt->concurrent;

    // Resolve table
    auto table_ref = resolveTable(stmt->table_path, stmt->span, false);
    if (!table_ref) {
        return nullptr;
    }
    resolved->table_uuid = table_ref->table_uuid;
    resolved->table_path = internString(schemaPathToString(stmt->table_path, string_pool_));

    if (stmt->has_tablespace) {
        resolved->tablespace_name = internString(schemaPathToString(stmt->tablespace, string_pool_));
    }

    if (!stmt->include_columns.empty()) {
        error(stmt->span, "INCLUDE columns are not supported for CREATE INDEX yet");
        return nullptr;
    }

    // Map index type to string
    switch (stmt->index_type) {
        case IndexType::BTREE: resolved->index_method = internString("btree"); break;
        case IndexType::HASH: resolved->index_method = internString("hash"); break;
        case IndexType::GIN: resolved->index_method = internString("gin"); break;
        case IndexType::GIST: resolved->index_method = internString("gist"); break;
        case IndexType::BRIN: resolved->index_method = internString("brin"); break;
        case IndexType::BITMAP: resolved->index_method = internString("bitmap"); break;
    }

    // Resolve index columns
    for (const auto& idx_col : stmt->columns) {
        if (idx_col.column != StringPool::INVALID_ID) {
            // Named column
            bool found = false;
            for (uint32_t i = 0; i < table_ref->columns.size(); ++i) {
                if (table_ref->columns[i].name == idx_col.column) {
                    resolved->column_indexes.push_back(i);
                    resolved->column_desc.push_back(!idx_col.ascending);
                    resolved->column_names.push_back(table_ref->columns[i].name);
                    found = true;
                    break;
                }
            }
            if (!found) {
                error(stmt->span, "Index column not found: " + std::string(getString(idx_col.column)));
                return nullptr;
            }
        } else if (idx_col.expr) {
            error(stmt->span, "Expression indexes are not supported yet");
            return nullptr;
        }
    }

    // Analyze WHERE clause for partial index
    if (stmt->where_clause) {
        error(stmt->span, "Partial indexes are not supported yet");
        return nullptr;
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateView(CreateViewStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateViewStmt>();
    resolved->span = stmt->span;
    resolved->or_replace = stmt->or_replace;
    resolved->materialized = stmt->materialized;

    // Resolve schema from view path
    if (stmt->view_path.components.size() >= 2) {
        std::string schema_name = std::string(string_pool_.get(stmt->view_path.components[0]));
        CatalogManager::SchemaInfo schema_info;
        if (catalog_.getSchema(schema_name, schema_info) == Status::OK) {
            resolved->schema.schema_uuid = schema_info.schema_id;
            resolved->schema.schema_name = stmt->view_path.components[0];
        }
        resolved->view_name = stmt->view_path.components[1];
    } else if (stmt->view_path.components.size() == 1) {
        resolved->schema.schema_uuid = current_schema_;
        resolved->view_name = stmt->view_path.components[0];
    }

    // Copy column names
    resolved->column_names = stmt->column_names;

    // Analyze the view query
    if (stmt->query) {
        resolved->query = analyzeSelect(static_cast<SelectStmt*>(stmt->query));
    }

    resolved->check_option = stmt->with_check_option;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateSchema(CreateSchemaStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateSchemaStmt>();
    resolved->span = stmt->span;
    resolved->if_not_exists = stmt->if_not_exists;
    resolved->schema_path = stmt->schema_path;
    resolved->owner = stmt->has_owner ? stmt->owner : StringPool::INVALID_ID;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropSchema(DropSchemaStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropSchemaStmt>();
    resolved->span = stmt->span;
    resolved->if_exists = stmt->if_exists;
    resolved->cascade = stmt->cascade;
    resolved->restrict = stmt->restrict;
    resolved->schema_paths = stmt->schemas;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateDatabase(CreateDatabaseStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateDatabaseStmt>();
    resolved->span = stmt->span;
    resolved->if_not_exists = stmt->if_not_exists;
    resolved->database_path = stmt->database_path;
    if (stmt->source_spec != StringPool::INVALID_ID)
    {
        resolved->source_spec = std::string(string_pool_.get(stmt->source_spec));
    }

    resolved->options.reserve(stmt->options.size());
    for (const auto& opt : stmt->options)
    {
        ResolvedDatabaseOption resolved_opt;
        if (opt.key != StringPool::INVALID_ID)
        {
            resolved_opt.key = std::string(string_pool_.get(opt.key));
        }
        if (opt.value != StringPool::INVALID_ID)
        {
            resolved_opt.value = std::string(string_pool_.get(opt.value));
        }
        resolved->options.push_back(std::move(resolved_opt));
    }

    resolved->aliases.reserve(stmt->aliases.size());
    for (auto alias_id : stmt->aliases)
    {
        if (alias_id == StringPool::INVALID_ID)
        {
            continue;
        }
        resolved->aliases.emplace_back(string_pool_.get(alias_id));
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateFunction(CreateFunctionStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateFunctionStmt>();
    resolved->span = stmt->span;
    resolved->or_replace = stmt->or_replace;
    resolved->deterministic = stmt->deterministic;
    resolved->sql_security = stmt->sql_security;

    if (stmt->function_path.components.size() >= 2) {
        std::string schema_name = std::string(string_pool_.get(stmt->function_path.components[0]));
        CatalogManager::SchemaInfo schema_info;
        if (catalog_.getSchema(schema_name, schema_info) == Status::OK) {
            resolved->schema.schema_uuid = schema_info.schema_id;
            resolved->schema.schema_name = stmt->function_path.components[0];
        }
        resolved->function_name = stmt->function_path.components[1];
    } else if (stmt->function_path.components.size() == 1) {
        resolved->schema.schema_uuid = current_schema_;
        resolved->function_name = stmt->function_path.components[0];
    }

    for (const auto& param : stmt->params) {
        ResolvedRoutineParam resolved_param;
        resolved_param.mode = static_cast<uint8_t>(param.mode);
        resolved_param.name = param.name;
        resolved_param.type = resolveTypeName(param.type);
        resolved->params.push_back(std::move(resolved_param));
    }

    resolved->return_type = resolveTypeName(stmt->return_type);
    if (stmt->body != StringPool::INVALID_ID) {
        resolved->body = std::string(string_pool_.get(stmt->body));
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateProcedure(CreateProcedureStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateProcedureStmt>();
    resolved->span = stmt->span;
    resolved->or_replace = stmt->or_replace;
    resolved->sql_security = stmt->sql_security;

    if (stmt->procedure_path.components.size() >= 2) {
        std::string schema_name = std::string(string_pool_.get(stmt->procedure_path.components[0]));
        CatalogManager::SchemaInfo schema_info;
        if (catalog_.getSchema(schema_name, schema_info) == Status::OK) {
            resolved->schema.schema_uuid = schema_info.schema_id;
            resolved->schema.schema_name = stmt->procedure_path.components[0];
        }
        resolved->procedure_name = stmt->procedure_path.components[1];
    } else if (stmt->procedure_path.components.size() == 1) {
        resolved->schema.schema_uuid = current_schema_;
        resolved->procedure_name = stmt->procedure_path.components[0];
    }

    for (const auto& param : stmt->params) {
        ResolvedRoutineParam resolved_param;
        resolved_param.mode = static_cast<uint8_t>(param.mode);
        resolved_param.name = param.name;
        resolved_param.type = resolveTypeName(param.type);
        resolved->params.push_back(std::move(resolved_param));
    }

    if (stmt->body != StringPool::INVALID_ID) {
        resolved->body = std::string(string_pool_.get(stmt->body));
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateTrigger(CreateTriggerStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateTriggerStmt>();
    resolved->span = stmt->span;
    resolved->trigger_name = stmt->trigger_name;
    resolved->table_path = stmt->table_path;
    resolved->active = stmt->active;
    resolved->timing = stmt->timing;
    resolved->event_mask = stmt->event_mask;
    resolved->granularity = stmt->granularity;
    if (stmt->body != StringPool::INVALID_ID) {
        resolved->body = std::string(string_pool_.get(stmt->body));
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCreatePackage(CreatePackageStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreatePackageStmt>();
    resolved->span = stmt->span;
    resolved->or_replace = stmt->or_replace;
    resolved->is_body = stmt->is_body;

    if (stmt->package_path.components.size() >= 2) {
        std::string schema_name = std::string(string_pool_.get(stmt->package_path.components[0]));
        CatalogManager::SchemaInfo schema_info;
        if (catalog_.getSchema(schema_name, schema_info) == Status::OK) {
            resolved->schema.schema_uuid = schema_info.schema_id;
            resolved->schema.schema_name = stmt->package_path.components[0];
        }
        resolved->package_name = stmt->package_path.components[1];
    } else if (stmt->package_path.components.size() == 1) {
        resolved->schema.schema_uuid = current_schema_;
        resolved->package_name = stmt->package_path.components[0];
    }

    if (stmt->header != StringPool::INVALID_ID) {
        resolved->header = std::string(string_pool_.get(stmt->header));
    }
    if (stmt->body != StringPool::INVALID_ID) {
        resolved->body = std::string(string_pool_.get(stmt->body));
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateRole(CreateRoleStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateRoleStmt>();
    resolved->span = stmt->span;
    resolved->role_name = stmt->role_name;
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateException(CreateExceptionStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateExceptionStmt>();
    resolved->span = stmt->span;
    resolved->or_replace = stmt->or_replace;

    if (stmt->exception_path.components.size() >= 2) {
        std::string schema_name = std::string(string_pool_.get(stmt->exception_path.components[0]));
        CatalogManager::SchemaInfo schema_info;
        if (catalog_.getSchema(schema_name, schema_info) == Status::OK) {
            resolved->schema.schema_uuid = schema_info.schema_id;
            resolved->schema.schema_name = stmt->exception_path.components[0];
        }
        resolved->exception_name = stmt->exception_path.components[1];
    } else if (stmt->exception_path.components.size() == 1) {
        resolved->schema.schema_uuid = current_schema_;
        resolved->exception_name = stmt->exception_path.components[0];
    }

    if (stmt->message != StringPool::INVALID_ID) {
        resolved->message = std::string(string_pool_.get(stmt->message));
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCreateDomain(CreateDomainStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCreateDomainStmt>();
    resolved->span = stmt->span;
    resolved->if_not_exists = stmt->if_not_exists;
    resolved->domain_path = stmt->domain_path;
    resolved->domain_kind = stmt->domain_kind;

    core::ObjectPath obj_path = buildObjectPath(stmt->domain_path, string_pool_);
    core::CatalogManager::ObjectType resolved_type = core::CatalogManager::ObjectType::UNKNOWN;
    core::ErrorContext ctx;
    core::CatalogManager::ResolveOptions opts;
    opts.allow_search_path = false;
    ID existing_id{};
    Status status = catalog_.resolveObjectPath(
        obj_path,
        core::CatalogManager::ObjectType::DOMAIN,
        opts,
        existing_id,
        resolved_type,
        &ctx);
    if (status == Status::OK) {
        if (!stmt->if_not_exists) {
            error(stmt->span, "Domain already exists");
            return nullptr;
        }
    } else if (status != Status::NOT_FOUND) {
        std::string msg = ctx.message.empty() ? "Failed to resolve domain" : ctx.message;
        error(stmt->span, msg);
        return nullptr;
    }

    resolved->nullable = true;
    bool default_set = false;
    bool nullable_explicit = false;
    std::unordered_set<std::string> constraint_names;
    for (const auto& constraint : stmt->constraints) {
        if (constraint.name != StringPool::INVALID_ID) {
            std::string name = core::IdentifierUtils::toUpper(
                std::string(string_pool_.get(constraint.name)));
            if (!constraint_names.insert(name).second) {
                error(constraint.span, "Duplicate domain constraint name");
                return nullptr;
            }
        }
        switch (constraint.type) {
            case DomainConstraintType::NOT_NULL:
                resolved->nullable = false;
                nullable_explicit = true;
                break;
            case DomainConstraintType::NULL_ALLOWED:
                resolved->nullable = true;
                nullable_explicit = true;
                break;
            case DomainConstraintType::DEFAULT:
                if (default_set) {
                    error(stmt->span, "Multiple DEFAULT clauses for domain");
                    break;
                }
                resolved->default_value = trimString(constraint.expression);
                default_set = true;
                break;
            case DomainConstraintType::CHECK: {
                std::string normalized = normalizeValueToken(constraint.expression);
                if (!hasTokenOutsideQuotes(normalized, "VALUE")) {
                    error(constraint.span, "CHECK constraint must reference VALUE");
                    return nullptr;
                }
                if (hasTokenOutsideQuotes(normalized, "SELECT")) {
                    error(constraint.span, "CHECK constraint subqueries are not supported");
                    return nullptr;
                }
                ResolvedDomainConstraint resolved_constraint;
                resolved_constraint.type = DomainConstraintType::CHECK;
                resolved_constraint.name = constraint.name;
                resolved_constraint.expression = std::move(normalized);
                resolved->constraints.push_back(std::move(resolved_constraint));
                break;
            }
            default:
                error(stmt->span, "Unsupported domain constraint");
                break;
        }
    }

    resolved->has_inherits = stmt->has_inherits;
    if (stmt->has_inherits) {
        core::ObjectPath obj_path = buildObjectPath(stmt->parent_domain_path, string_pool_);
        core::CatalogManager::ObjectType resolved_type = core::CatalogManager::ObjectType::UNKNOWN;
        core::CatalogManager::ResolveOptions opts;
        opts.allow_search_path = true;
        Status status = catalog_.resolveObjectPath(
            obj_path,
            core::CatalogManager::ObjectType::DOMAIN,
            opts,
            resolved->parent_domain_id,
            resolved_type,
            &ctx);

        if (status != Status::OK) {
            std::string msg = ctx.message.empty() ? "Parent domain not found" : ctx.message;
            error(stmt->parent_domain_path.span, msg);
            return nullptr;
        }
        if (current_result_) {
            current_result_->addDependency(resolved->parent_domain_id,
                                           core::CatalogManager::ObjectType::DOMAIN);
        }
    }

    resolved->has_collation = stmt->has_collation;
    resolved->collation_name = stmt->collation_name;
    resolved->has_dialect = stmt->has_dialect;
    resolved->dialect_tag = stmt->dialect_tag;
    resolved->has_compat = stmt->has_compat;
    resolved->compat_name = stmt->compat_name;
    resolved->enum_wrap = stmt->enum_wrap;

    if (stmt->enum_wrap && stmt->domain_kind != DomainKind::ENUM) {
        error(stmt->span, "WITH OPTIONS is only valid for ENUM domains");
        return nullptr;
    }

    switch (stmt->domain_kind) {
        case DomainKind::BASIC: {
            resolved->base_type = resolveTypeName(stmt->base_type);
            if (resolved->base_type.is_domain) {
                if (resolved->has_inherits) {
                    error(stmt->span, "INHERITS cannot be combined with domain base type");
                    return nullptr;
                }
                resolved->has_inherits = true;
                resolved->parent_domain_id = resolved->base_type.domain_id;
                resolved->base_type.is_domain = false;
                resolved->base_type.domain_id = ID{};
            }
            if (resolved->base_type.data_type == DataType::UNKNOWN) {
                error(stmt->base_type.span, "Unknown base type for domain");
                return nullptr;
            }
            break;
        }
        case DomainKind::RECORD: {
            if (stmt->record_fields.empty()) {
                error(stmt->span, "RECORD domain must define at least one field");
                return nullptr;
            }
            std::unordered_set<std::string> field_names;
            for (const auto& field : stmt->record_fields) {
                std::string name = core::IdentifierUtils::toUpper(
                    std::string(string_pool_.get(field.name)));
                if (!field_names.insert(name).second) {
                    error(field.span, "Duplicate RECORD field name");
                    return nullptr;
                }
                if (field.type.has_schema_path &&
                    schemaPathEquals(field.type.schema_path, stmt->domain_path, string_pool_)) {
                    error(field.span, "RECORD domain cannot reference itself");
                    return nullptr;
                }
                ResolvedDomainRecordField resolved_field;
                resolved_field.name = field.name;
                resolved_field.type = resolveTypeName(field.type);
                resolved_field.nullable = field.nullable;
                resolved_field.default_value = trimString(field.default_value);
                if (resolved_field.type.data_type == DataType::UNKNOWN && !resolved_field.type.is_domain) {
                    error(field.type.span, "Unknown field type for RECORD domain");
                    return nullptr;
                }
                if (field.has_default) {
                    LiteralKind lit_kind = classifyLiteral(resolved_field.default_value);
                    if (!literalCompatibleWithType(lit_kind, resolved_field.type.data_type)) {
                        error(field.span, "RECORD field default does not match field type");
                        return nullptr;
                    }
                }
                resolved->record_fields.push_back(std::move(resolved_field));
            }
            break;
        }
        case DomainKind::ENUM: {
            if (stmt->enum_values.empty()) {
                error(stmt->span, "ENUM domain must have at least one value");
                return nullptr;
            }
            int32_t next_position = 1;
            std::unordered_set<std::string> enum_labels;
            for (const auto& value : stmt->enum_values) {
                std::string label = std::string(string_pool_.get(value.label));
                if (!enum_labels.insert(label).second) {
                    error(value.span, "Duplicate ENUM label");
                    return nullptr;
                }
                ResolvedDomainEnumValue resolved_value;
                resolved_value.label = value.label;
                if (value.has_position) {
                    if (value.position != next_position) {
                        error(value.span, "ENUM positions must be sequential starting from 1");
                        return nullptr;
                    }
                    resolved_value.position = value.position;
                } else {
                    resolved_value.position = next_position;
                }
                resolved->enum_values.push_back(std::move(resolved_value));
                next_position++;
            }
            break;
        }
        case DomainKind::SET: {
            resolved->set_element_type = resolveTypeName(stmt->set_element_type);
            if (resolved->set_element_type.data_type == DataType::UNKNOWN &&
                !resolved->set_element_type.is_domain) {
                error(stmt->set_element_type.span, "Unknown element type for SET domain");
                return nullptr;
            }
            break;
        }
        case DomainKind::VARIANT: {
            if (stmt->variant_allowed_types.empty()) {
                error(stmt->span, "VARIANT domain must define at least one type");
                return nullptr;
            }
            for (const auto& type_ref : stmt->variant_allowed_types) {
                ResolvedType resolved_type = resolveTypeName(type_ref);
                if (resolved_type.data_type == DataType::UNKNOWN && !resolved_type.is_domain) {
                    error(type_ref.span, "Unknown allowed type for VARIANT domain");
                    return nullptr;
                }
                for (const auto& existing : resolved->variant_allowed_types) {
                    if (typesEquivalent(resolved_type, existing)) {
                        error(type_ref.span, "Duplicate VARIANT allowed type");
                        return nullptr;
                    }
                }
                resolved->variant_allowed_types.push_back(std::move(resolved_type));
            }
            break;
        }
    }

    if (resolved->has_inherits) {
        SourceSpan inherits_span = stmt->has_inherits
            ? stmt->parent_domain_path.span
            : stmt->base_type.span;
        std::unordered_set<core::ID, core::IDHash> visited;
        core::ID current = resolved->parent_domain_id;
        size_t depth = 0;
        bool checked_parent = false;
        core::DomainInfo parent_info;

        while (current != core::ID()) {
            if (!visited.insert(current).second) {
                error(inherits_span, "Circular domain inheritance detected");
                return nullptr;
            }
            depth++;
            if (depth > kMaxDomainInheritanceDepth) {
                error(inherits_span, "Domain inheritance depth exceeds maximum (10)");
                return nullptr;
            }
            if (catalog_.getDomainById(current, parent_info, &ctx) != Status::OK) {
                std::string msg = ctx.message.empty() ? "Failed to load parent domain" : ctx.message;
                error(inherits_span, msg);
                return nullptr;
            }
            if (!checked_parent) {
                if (toCoreDomainType(stmt->domain_kind) != parent_info.domain_type) {
                    error(inherits_span, "Parent domain type does not match child domain type");
                    return nullptr;
                }
                if (stmt->domain_kind == DomainKind::BASIC &&
                    !baseTypeMatchesParent(resolved->base_type, parent_info)) {
                    error(inherits_span, "Inherited domain base type must match parent domain");
                    return nullptr;
                }
                if (!parent_info.nullable) {
                    if (nullable_explicit && resolved->nullable) {
                        error(inherits_span, "Cannot relax NOT NULL inherited from parent domain");
                        return nullptr;
                    }
                    resolved->nullable = false;
                }
                checked_parent = true;
            }
            current = parent_info.parent_domain_id;
        }
    }

    if (default_set && stmt->domain_kind == DomainKind::BASIC) {
        LiteralKind lit_kind = classifyLiteral(resolved->default_value);
        if (!literalCompatibleWithType(lit_kind, resolved->base_type.data_type)) {
            error(stmt->span, "DEFAULT value does not match domain base type");
            return nullptr;
        }
    }

    resolved->has_integrity = stmt->has_integrity;
    resolved->integrity = stmt->integrity;
    resolved->has_security = stmt->has_security;
    resolved->security = stmt->security;
    resolved->has_validation = stmt->has_validation;
    resolved->validation = stmt->validation;
    resolved->has_quality = stmt->has_quality;
    resolved->quality = stmt->quality;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropDatabase(DropDatabaseStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropDatabaseStmt>();
    resolved->span = stmt->span;
    resolved->if_exists = stmt->if_exists;
    resolved->force = stmt->force;
    resolved->database_path = stmt->database_path;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeAlterSchema(AlterSchemaStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedAlterSchemaStmt>();
    resolved->span = stmt->span;
    resolved->action = stmt->action;
    resolved->schema_path = stmt->schema_path;
    resolved->new_name = stmt->new_name;
    resolved->owner = stmt->owner;
    resolved->new_path = stmt->new_path;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeAlterDatabase(AlterDatabaseStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedAlterDatabaseStmt>();
    resolved->span = stmt->span;
    resolved->action = stmt->action;
    resolved->database_path = stmt->database_path;
    resolved->new_name = stmt->new_name;
    resolved->owner = stmt->owner;
    if (stmt->alias != StringPool::INVALID_ID)
    {
        resolved->alias = std::string(string_pool_.get(stmt->alias));
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeAlterDomain(AlterDomainStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedAlterDomainStmt>();
    resolved->span = stmt->span;
    resolved->action = stmt->action;
    resolved->domain_path = stmt->domain_path;
    resolved->value = stmt->value;
    resolved->constraint_name = stmt->constraint_name;
    resolved->new_name = stmt->new_name;

    core::ObjectPath obj_path = buildObjectPath(stmt->domain_path, string_pool_);
    core::CatalogManager::ObjectType resolved_type = core::CatalogManager::ObjectType::UNKNOWN;
    core::ErrorContext ctx;
    core::CatalogManager::ResolveOptions opts;
    opts.allow_search_path = false;
    ID domain_id{};
    Status status = catalog_.resolveObjectPath(
        obj_path,
        core::CatalogManager::ObjectType::DOMAIN,
        opts,
        domain_id,
        resolved_type,
        &ctx);
    if (status != Status::OK) {
        std::string msg = ctx.message.empty() ? "Domain not found" : ctx.message;
        error(stmt->span, msg);
        return nullptr;
    }

    core::DomainInfo dinfo;
    if (catalog_.getDomainById(domain_id, dinfo, &ctx) != Status::OK) {
        std::string msg = ctx.message.empty() ? "Failed to load domain" : ctx.message;
        error(stmt->span, msg);
        return nullptr;
    }

    switch (stmt->action) {
        case AlterDomainAction::ADD_CHECK: {
            std::string normalized = normalizeValueToken(stmt->value);
            if (!hasTokenOutsideQuotes(normalized, "VALUE")) {
                error(stmt->span, "CHECK constraint must reference VALUE");
                return nullptr;
            }
            if (hasTokenOutsideQuotes(normalized, "SELECT")) {
                error(stmt->span, "CHECK constraint subqueries are not supported");
                return nullptr;
            }
            resolved->value = std::move(normalized);
            break;
        }
        case AlterDomainAction::DROP_CONSTRAINT: {
            if (stmt->constraint_name == StringPool::INVALID_ID) {
                error(stmt->span, "Expected constraint name");
                return nullptr;
            }
            std::string target = core::IdentifierUtils::toUpper(
                std::string(string_pool_.get(stmt->constraint_name)));
            bool found = false;
            for (const auto& constraint : dinfo.constraints) {
                if (constraint.name.empty()) {
                    continue;
                }
                std::string existing = core::IdentifierUtils::toUpper(constraint.name);
                if (existing == target) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                error(stmt->span, "Domain constraint not found");
                return nullptr;
            }
            break;
        }
        case AlterDomainAction::SET_DEFAULT: {
            std::string trimmed = trimString(stmt->value);
            if (trimmed.empty()) {
                error(stmt->span, "DEFAULT value cannot be empty");
                return nullptr;
            }
            if (dinfo.domain_type == core::DomainType::BASIC) {
                LiteralKind lit_kind = classifyLiteral(trimmed);
                if (!literalCompatibleWithType(lit_kind, dinfo.base_type)) {
                    error(stmt->span, "DEFAULT value does not match domain base type");
                    return nullptr;
                }
            }
            resolved->value = std::move(trimmed);
            break;
        }
        case AlterDomainAction::RENAME: {
            if (stmt->new_name == StringPool::INVALID_ID) {
                error(stmt->span, "Expected new domain name");
                return nullptr;
            }
            SchemaPath new_path = stmt->domain_path;
            if (!new_path.components.empty()) {
                new_path.components.back() = stmt->new_name;
            }
            new_path.span = stmt->span;

            core::ObjectPath new_obj = buildObjectPath(new_path, string_pool_);
            core::CatalogManager::ObjectType new_type = core::CatalogManager::ObjectType::UNKNOWN;
            core::ErrorContext rename_ctx;
            ID new_id{};
            Status rename_status = catalog_.resolveObjectPath(
                new_obj,
                core::CatalogManager::ObjectType::DOMAIN,
                opts,
                new_id,
                new_type,
                &rename_ctx);
            if (rename_status == Status::OK) {
                error(stmt->span, "Domain name already exists");
                return nullptr;
            }
            if (rename_status != Status::NOT_FOUND) {
                std::string msg = rename_ctx.message.empty() ? "Failed to resolve new domain name"
                                                             : rename_ctx.message;
                error(stmt->span, msg);
                return nullptr;
            }
            break;
        }
        default:
            break;
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropDomain(DropDomainStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropDomainStmt>();
    resolved->span = stmt->span;
    resolved->if_exists = stmt->if_exists;
    resolved->domains = stmt->domains;
    resolved->restrict = stmt->restrict;

    core::CatalogManager::ResolveOptions opts;
    opts.allow_search_path = false;
    for (const auto& path : stmt->domains) {
        core::ObjectPath obj_path = buildObjectPath(path, string_pool_);
        core::CatalogManager::ObjectType resolved_type = core::CatalogManager::ObjectType::UNKNOWN;
        core::ErrorContext ctx;
        ID domain_id{};
        Status status = catalog_.resolveObjectPath(
            obj_path,
            core::CatalogManager::ObjectType::DOMAIN,
            opts,
            domain_id,
            resolved_type,
            &ctx);
        if (status == Status::NOT_FOUND && stmt->if_exists) {
            continue;
        }
        if (status != Status::OK) {
            std::string msg = ctx.message.empty() ? "Domain not found" : ctx.message;
            error(path.span, msg);
            return nullptr;
        }

        std::vector<std::pair<ID, std::string>> table_columns;
        if (catalog_.findColumnsByDomain(domain_id, table_columns, &ctx) != Status::OK) {
            std::string msg = ctx.message.empty() ? "Failed to check domain dependencies" : ctx.message;
            error(path.span, msg);
            return nullptr;
        }
        if (!table_columns.empty()) {
            error(path.span, "Cannot drop domain referenced by table columns");
            return nullptr;
        }

        std::vector<core::DomainInfo> child_domains;
        if (catalog_.findChildDomains(domain_id, child_domains, &ctx) != Status::OK) {
            std::string msg = ctx.message.empty() ? "Failed to check domain inheritance" : ctx.message;
            error(path.span, msg);
            return nullptr;
        }
        if (!child_domains.empty()) {
            error(path.span, "Cannot drop domain with child domains");
            return nullptr;
        }
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeRenameObject(RenameObjectStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedRenameObjectStmt>();
    resolved->span = stmt->span;
    resolved->object_type = stmt->object_type;
    resolved->if_exists = stmt->if_exists;
    resolved->object_path = stmt->object_path;
    resolved->new_name = stmt->new_name;
    resolved->has_uuid = false;

    core::ObjectPath obj_path = buildObjectPath(stmt->object_path, string_pool_);
    core::CatalogManager::ObjectType resolved_type = core::CatalogManager::ObjectType::UNKNOWN;
    core::ErrorContext err_ctx;
    core::CatalogManager::ResolveOptions opts;
    opts.allow_search_path = false;
    Status status = catalog_.resolveObjectPath(
        obj_path,
        toCatalogObjectType(stmt->object_type),
        opts,
        resolved->object_uuid,
        resolved_type,
        &err_ctx);

    if (status == Status::OK) {
        resolved->has_uuid = true;
    } else if (!(status == Status::NOT_FOUND && stmt->if_exists)) {
        std::string msg = err_ctx.message.empty() ? "Failed to resolve object" : err_ctx.message;
        error(stmt->span, msg);
        return nullptr;
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeMoveObject(MoveObjectStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedMoveObjectStmt>();
    resolved->span = stmt->span;
    resolved->object_type = stmt->object_type;
    resolved->if_exists = stmt->if_exists;
    resolved->object_path = stmt->object_path;
    resolved->target_schema = stmt->target_schema;
    resolved->has_new_name = stmt->has_new_name;
    resolved->new_name = stmt->new_name;
    resolved->has_uuid = false;

    core::ObjectPath obj_path = buildObjectPath(stmt->object_path, string_pool_);
    core::CatalogManager::ObjectType resolved_type = core::CatalogManager::ObjectType::UNKNOWN;
    core::ErrorContext err_ctx;
    core::CatalogManager::ResolveOptions opts;
    opts.allow_search_path = false;
    Status status = catalog_.resolveObjectPath(
        obj_path,
        toCatalogObjectType(stmt->object_type),
        opts,
        resolved->object_uuid,
        resolved_type,
        &err_ctx);

    if (status == Status::OK) {
        resolved->has_uuid = true;
    } else if (!(status == Status::NOT_FOUND && stmt->if_exists)) {
        std::string msg = err_ctx.message.empty() ? "Failed to resolve object" : err_ctx.message;
        error(stmt->span, msg);
        return nullptr;
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeAlterTable(AlterTableStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto resolve_rename = [&](DdlObjectType type,
                              const SchemaPath& path,
                              StringPool::StringId new_name) -> ResolvedStatement* {
        auto* resolved = arena_.create<ResolvedRenameObjectStmt>();
        resolved->span = stmt->span;
        resolved->object_type = type;
        resolved->if_exists = stmt->if_exists;
        resolved->object_path = path;
        resolved->new_name = new_name;
        resolved->has_uuid = false;

        core::ObjectPath obj_path = buildObjectPath(path, string_pool_);
        core::CatalogManager::ObjectType resolved_type = core::CatalogManager::ObjectType::UNKNOWN;
        core::ErrorContext err_ctx;
        core::CatalogManager::ResolveOptions opts;
        opts.allow_search_path = false;
        Status status = catalog_.resolveObjectPath(
            obj_path,
            toCatalogObjectType(type),
            opts,
            resolved->object_uuid,
            resolved_type,
            &err_ctx);

        if (status == Status::OK) {
            resolved->has_uuid = true;
        } else if (!(status == Status::NOT_FOUND && stmt->if_exists)) {
            std::string msg = err_ctx.message.empty() ? "Failed to resolve object" : err_ctx.message;
            error(stmt->span, msg);
            return nullptr;
        }

        return resolved;
    };

    auto resolve_move = [&](const SchemaPath& path,
                            const SchemaPath& target_schema) -> ResolvedStatement* {
        auto* resolved = arena_.create<ResolvedMoveObjectStmt>();
        resolved->span = stmt->span;
        resolved->object_type = DdlObjectType::TABLE;
        resolved->if_exists = stmt->if_exists;
        resolved->object_path = path;
        resolved->target_schema = target_schema;
        resolved->has_uuid = false;

        core::ObjectPath obj_path = buildObjectPath(path, string_pool_);
        core::CatalogManager::ObjectType resolved_type = core::CatalogManager::ObjectType::UNKNOWN;
        core::ErrorContext err_ctx;
        core::CatalogManager::ResolveOptions opts;
        opts.allow_search_path = false;
        Status status = catalog_.resolveObjectPath(
            obj_path,
            toCatalogObjectType(DdlObjectType::TABLE),
            opts,
            resolved->object_uuid,
            resolved_type,
            &err_ctx);

        if (status == Status::OK) {
            resolved->has_uuid = true;
        } else if (!(status == Status::NOT_FOUND && stmt->if_exists)) {
            std::string msg = err_ctx.message.empty() ? "Failed to resolve object" : err_ctx.message;
            error(stmt->span, msg);
            return nullptr;
        }

        return resolved;
    };

    switch (stmt->action) {
        case AlterTableAction::RENAME_TABLE:
            return resolve_rename(DdlObjectType::TABLE, stmt->table_path, stmt->new_name);
        case AlterTableAction::RENAME_COLUMN: {
            SchemaPath full_path = appendPathComponent(stmt->table_path, stmt->column_name, stmt->span);
            return resolve_rename(DdlObjectType::COLUMN, full_path, stmt->new_name);
        }
        case AlterTableAction::RENAME_CONSTRAINT: {
            SchemaPath full_path = appendPathComponent(stmt->table_path, stmt->constraint_name, stmt->span);
            return resolve_rename(DdlObjectType::CONSTRAINT, full_path, stmt->new_name);
        }
        case AlterTableAction::SET_SCHEMA:
            return resolve_move(stmt->table_path, stmt->target_schema);
        default:
            break;
    }

    auto table_ref = resolveTable(stmt->table_path, stmt->span, false);
    if (!table_ref) {
        return nullptr;
    }

    if (table_ref->object_type != ResolvedTableRef::ObjectType::TABLE) {
        error(stmt->span, "ALTER TABLE requires a base table");
        return nullptr;
    }

    core::ErrorContext err_ctx;
    std::string schema_path;
    if (catalog_.getSchemaPath(table_ref->schema_uuid, schema_path, &err_ctx) != Status::OK) {
        std::string msg = err_ctx.message.empty() ? "Failed to resolve schema path" : err_ctx.message;
        error(stmt->span, msg);
        return nullptr;
    }

    std::string table_name = std::string(getString(table_ref->name));
    std::string display_schema_path = stripRootPrefixForDisplay(schema_path);
    std::string qualified_name =
        display_schema_path.empty() ? table_name : display_schema_path + "." + table_name;

    if (stmt->only) {
        warning(stmt->span, "ALTER TABLE ONLY is not supported");
    }
    if (stmt->if_exists) {
        warning(stmt->span, "ALTER TABLE IF EXISTS is not enforced at bytecode level");
    }

    auto* resolved = arena_.create<ResolvedAlterTableStmt>();
    resolved->span = stmt->span;
    resolved->action = stmt->action;
    resolved->if_exists = stmt->if_exists;
    resolved->only = stmt->only;
    resolved->cascade = stmt->cascade;
    resolved->table_uuid = table_ref->table_uuid;
    resolved->schema_uuid = table_ref->schema_uuid;
    resolved->table_name = table_ref->name;
    resolved->qualified_table_name = internString(qualified_name);

    switch (stmt->action) {
        case AlterTableAction::ADD_COLUMN: {
            if (!stmt->column) {
                error(stmt->span, "ALTER TABLE ADD COLUMN requires a column definition");
                return nullptr;
            }
            if (stmt->column->is_computed || stmt->column->computed_expr) {
                error(stmt->span, "ALTER TABLE ADD COLUMN does not support computed columns");
                return nullptr;
            }
            for (const auto& constraint : stmt->column->constraints) {
                if (constraint.type == ConstraintType::NOT_NULL ||
                    constraint.type == ConstraintType::NULL_ALLOWED) {
                    continue;
                }
                error(stmt->span, "ALTER TABLE ADD COLUMN supports only NULL/NOT NULL constraints");
                return nullptr;
            }

            ResolvedColumnDef col_def = analyzeColumnDef(stmt->column);
            if (col_def.name == StringPool::INVALID_ID) {
                error(stmt->span, "ALTER TABLE ADD COLUMN requires a column name");
                return nullptr;
            }
            if (col_def.type.data_type == DataType::UNKNOWN) {
                error(stmt->span, "ALTER TABLE ADD COLUMN has unsupported data type");
                return nullptr;
            }
            if (col_def.default_value || col_def.check_expr || col_def.is_primary_key ||
                col_def.is_unique || col_def.has_fk) {
                error(stmt->span, "ALTER TABLE ADD COLUMN supports only type and NULL/NOT NULL");
                return nullptr;
            }

            core::CatalogManager::ColumnInfo existing;
            if (catalog_.getColumn(table_ref->table_uuid, std::string(getString(col_def.name)),
                                   existing, &err_ctx) == Status::OK) {
                error(stmt->span, "Column already exists: " + std::string(getString(col_def.name)));
                return nullptr;
            }

            resolved->column_def = col_def;
            resolved->has_column_def = true;
            resolved->column_name = col_def.name;
            return resolved;
        }
        case AlterTableAction::DROP_COLUMN: {
            if (stmt->column_name == StringPool::INVALID_ID) {
                error(stmt->span, "ALTER TABLE DROP COLUMN requires a column name");
                return nullptr;
            }

            core::CatalogManager::ColumnInfo existing;
            if (catalog_.getColumn(table_ref->table_uuid, std::string(getString(stmt->column_name)),
                                   existing, &err_ctx) != Status::OK) {
                error(stmt->span, "Column not found: " + std::string(getString(stmt->column_name)));
                return nullptr;
            }

            resolved->column_name = stmt->column_name;
            return resolved;
        }
        case AlterTableAction::ALTER_COLUMN: {
            if (!stmt->column && stmt->column_name == StringPool::INVALID_ID) {
                error(stmt->span, "ALTER TABLE ALTER COLUMN requires a column name");
                return nullptr;
            }

            StringPool::StringId col_name = stmt->column_name;
            if (col_name == StringPool::INVALID_ID && stmt->column) {
                col_name = stmt->column->name;
            }
            if (col_name == StringPool::INVALID_ID) {
                error(stmt->span, "ALTER TABLE ALTER COLUMN requires a column name");
                return nullptr;
            }

            core::CatalogManager::ColumnInfo existing;
            if (catalog_.getColumn(table_ref->table_uuid, std::string(getString(col_name)),
                                   existing, &err_ctx) != Status::OK) {
                error(stmt->span, "Column not found: " + std::string(getString(col_name)));
                return nullptr;
            }

            if (!stmt->column) {
                error(stmt->span, "ALTER TABLE ALTER COLUMN requires a type definition");
                return nullptr;
            }
            if (stmt->column->is_computed || stmt->column->computed_expr) {
                error(stmt->span, "ALTER TABLE ALTER COLUMN does not support computed columns");
                return nullptr;
            }
            if (!stmt->column->constraints.empty()) {
                error(stmt->span, "ALTER TABLE ALTER COLUMN supports only type changes");
                return nullptr;
            }

            ResolvedColumnDef col_def = analyzeColumnDef(stmt->column);
            if (col_def.type.data_type == DataType::UNKNOWN) {
                error(stmt->span, "ALTER TABLE ALTER COLUMN has unsupported data type");
                return nullptr;
            }
            if (col_def.default_value || col_def.check_expr || col_def.is_primary_key ||
                col_def.is_unique || col_def.has_fk) {
                error(stmt->span, "ALTER TABLE ALTER COLUMN supports only type changes");
                return nullptr;
            }

            col_def.name = col_name;
            resolved->column_def = col_def;
            resolved->has_column_def = true;
            resolved->column_name = col_name;
            return resolved;
        }
        case AlterTableAction::SET_TABLESPACE: {
            if (stmt->tablespace.components.empty()) {
                error(stmt->span, "ALTER TABLE SET TABLESPACE requires a tablespace name");
                return nullptr;
            }
            if (stmt->tablespace.components.size() > 1) {
                warning(stmt->span, "Tablespace paths are global; using the final component");
            }
            resolved->tablespace_name = stmt->tablespace.components.back();
            resolved->tablespace_online = false;
            return resolved;
        }
        case AlterTableAction::ENABLE_RLS:
            resolved->rls_action = 0;
            return resolved;
        case AlterTableAction::DISABLE_RLS:
            resolved->rls_action = 1;
            return resolved;
        case AlterTableAction::ADD_CONSTRAINT:
        case AlterTableAction::DROP_CONSTRAINT:
            error(stmt->span, "ALTER TABLE constraint operations are not supported");
            return nullptr;
        default:
            break;
    }

    warning(stmt->span, "ALTER TABLE semantic analysis not fully implemented");
    return nullptr;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropTable(DropTableStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropStmt>();
    resolved->span = stmt->span;
    resolved->object_type = ResolvedDropStmt::ObjectType::TABLE;
    resolved->if_exists = stmt->if_exists;
    resolved->cascade = stmt->cascade;

    // Resolve each table
    for (const auto& table_path : stmt->tables) {
        resolved->object_paths.push_back(table_path);
        auto table_ref = resolveTable(table_path, stmt->span, false);
        if (table_ref) {
        } else if (!stmt->if_exists) {
            // Error already reported by resolveTable
        }
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeAlterIndex(AlterIndexStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedAlterIndexStmt>();
    resolved->span = stmt->span;
    resolved->index_path = stmt->index_path;
    resolved->active = (stmt->action == AlterIndexAction::ACTIVE);

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropIndex(DropIndexStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropStmt>();
    resolved->span = stmt->span;
    resolved->object_type = ResolvedDropStmt::ObjectType::INDEX;
    resolved->if_exists = stmt->if_exists;
    resolved->cascade = stmt->cascade;

    // Index resolution would require looking up indexes in catalog
    // For now, we just note the index names
    for (const auto& index_path : stmt->indexes) {
        resolved->object_paths.push_back(index_path);
        if (!index_path.components.empty()) {
            // Would need catalog_.getIndex() or similar
            warning(stmt->span, "Index resolution not fully implemented");
        }
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropView(DropViewStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropStmt>();
    resolved->span = stmt->span;
    resolved->object_type = ResolvedDropStmt::ObjectType::VIEW;
    resolved->if_exists = stmt->if_exists;
    resolved->cascade = stmt->cascade;

    // View resolution - views are treated like tables in catalog
    for (const auto& view_path : stmt->views) {
        resolved->object_paths.push_back(view_path);
        auto view_ref = resolveTable(view_path, stmt->span, false);
        if (view_ref) {
        } else if (!stmt->if_exists) {
            // Error already reported
        }
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropSequence(DropSequenceStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropStmt>();
    resolved->span = stmt->span;
    resolved->object_type = ResolvedDropStmt::ObjectType::SEQUENCE;
    resolved->if_exists = stmt->if_exists;
    resolved->cascade = stmt->cascade;

    for (const auto& sequence_path : stmt->sequences) {
        resolved->object_paths.push_back(sequence_path);
    }

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropFunction(DropFunctionStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropStmt>();
    resolved->span = stmt->span;
    resolved->object_type = ResolvedDropStmt::ObjectType::FUNCTION;
    resolved->if_exists = stmt->if_exists;
    resolved->cascade = false;
    resolved->object_paths = stmt->functions;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropProcedure(DropProcedureStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropStmt>();
    resolved->span = stmt->span;
    resolved->object_type = ResolvedDropStmt::ObjectType::PROCEDURE;
    resolved->if_exists = stmt->if_exists;
    resolved->cascade = false;
    resolved->object_paths = stmt->procedures;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropTrigger(DropTriggerStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropStmt>();
    resolved->span = stmt->span;
    resolved->object_type = ResolvedDropStmt::ObjectType::TRIGGER;
    resolved->if_exists = stmt->if_exists;
    resolved->cascade = false;
    resolved->object_paths = stmt->triggers;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropPackage(DropPackageStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropStmt>();
    resolved->span = stmt->span;
    resolved->object_type = ResolvedDropStmt::ObjectType::PACKAGE;
    resolved->if_exists = stmt->if_exists;
    resolved->cascade = false;
    resolved->object_paths = stmt->packages;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropRole(DropRoleStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropStmt>();
    resolved->span = stmt->span;
    resolved->object_type = ResolvedDropStmt::ObjectType::ROLE;
    resolved->if_exists = stmt->if_exists;
    resolved->cascade = stmt->cascade;
    resolved->object_paths = stmt->roles;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDropException(DropExceptionStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDropStmt>();
    resolved->span = stmt->span;
    resolved->object_type = ResolvedDropStmt::ObjectType::EXCEPTION;
    resolved->if_exists = stmt->if_exists;
    resolved->cascade = false;
    resolved->object_paths = stmt->exceptions;

    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeTruncateTable(TruncateTableStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedTruncateTableStmt>();
    resolved->span = stmt->span;
    resolved->cascade = stmt->cascade;
    resolved->restart_identity = stmt->restart_identity;
    resolved->async_mode = !stmt->sync_mode;  // ASYNC is default (sync_mode = false)

    for (const auto& table_path : stmt->tables) {
        resolved->table_paths.push_back(table_path);
        auto table_ref = resolveTable(table_path, stmt->span, false);
        if (table_ref) {
        } else {
            // Table resolution failed - error already reported
            return nullptr;
        }
    }

    if (resolved->table_paths.empty()) {
        error(stmt->span, "TRUNCATE TABLE requires at least one table");
        return nullptr;
    }

    return resolved;
}

// =============================================================================
// DML Statement Analysis
// =============================================================================

ResolvedSelectStmt* SemanticAnalyzerV2::analyzeSelect(SelectStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedSelectStmt>();
    resolved->span = stmt->span;
    resolved->distinct = stmt->distinct;
    resolved->all = stmt->all;
    resolved->for_update = stmt->for_update;
    resolved->for_share = stmt->for_share;

    // Push a new scope for this SELECT
    pushScope();

    // 1. Analyze FROM clause first (populates scope with table columns)
    analyzeFromClause(stmt, resolved);

    // 2. Analyze SELECT list
    analyzeSelectList(stmt, resolved);

    // 3. Analyze WHERE clause
    if (stmt->where) {
        resolved->where = analyzeExpression(stmt->where);
        if (resolved->where && !resolved->where->type.isBoolean() &&
            resolved->where->type.data_type != DataType::UNKNOWN) {
            error(stmt->where->span, "WHERE clause must be a boolean expression");
        }
    }

    // 4. Analyze GROUP BY clause
    analyzeGroupByClause(stmt, resolved);

    // 5. Analyze HAVING clause
    if (stmt->having) {
        resolved->having = analyzeExpression(stmt->having);
        if (resolved->having && !resolved->having->type.isBoolean() &&
            resolved->having->type.data_type != DataType::UNKNOWN) {
            error(stmt->having->span, "HAVING clause must be a boolean expression");
        }
    }

    // 6. Analyze ORDER BY clause
    analyzeOrderByClause(stmt->order_by, resolved->order_by);

    // 7. Analyze LIMIT/OFFSET
    if (stmt->limit) {
        resolved->limit = analyzeExpression(stmt->limit);
    }
    if (stmt->offset) {
        resolved->offset = analyzeExpression(stmt->offset);
    }

    // 8. Analyze set operations (UNION, INTERSECT, EXCEPT)
    if (stmt->set_op != SetOpType::NONE && stmt->set_op_right) {
        resolved->set_op = stmt->set_op;
        resolved->set_op_all = stmt->set_op_all;
        resolved->set_op_right = analyzeSelect(stmt->set_op_right);

        // Verify column count matches
        if (resolved->set_op_right &&
            resolved->select_list.size() != resolved->set_op_right->select_list.size()) {
            error(stmt->set_op_right->span, "Set operation queries must have same number of columns");
        }
    }

    // Validate GROUP BY semantics
    if (!resolved->group_by.empty() || has_aggregates_) {
        validateGroupBy(resolved);
    }

    popScope();
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeInsert(InsertStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedInsertStmt>();
    resolved->span = stmt->span;

    // Resolve target table
    auto table_ref = resolveTable(stmt->table_path, stmt->span);
    if (!table_ref) {
        return nullptr;
    }
    resolved->target_table = *table_ref;

    // Add table to scope for RETURNING clause
    pushScope();
    ResolutionScope::TableEntry entry;
    entry.table_uuid = table_ref->table_uuid;
    entry.columns = table_ref->columns;
    entry.alias = StringPool::INVALID_ID;  // No alias for target table
    currentScope().addTable(entry);

    // Resolve target columns
    if (stmt->columns.empty()) {
        // All columns in table order
        for (uint32_t i = 0; i < table_ref->columns.size(); ++i) {
            resolved->target_column_indexes.push_back(i);
        }
    } else {
        // Specified columns
        for (auto col_name : stmt->columns) {
            bool found = false;
            for (uint32_t i = 0; i < table_ref->columns.size(); ++i) {
                if (table_ref->columns[i].name == col_name) {
                    resolved->target_column_indexes.push_back(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                error(stmt->span, "Column not found: " + std::string(getString(col_name)));
                return nullptr;
            }
        }
    }

    // Analyze source based on InsertStmt::Source enum
    switch (stmt->source) {
        case InsertStmt::Source::SELECT:
            resolved->source = ResolvedInsertStmt::Source::SELECT;
            resolved->select_source = analyzeSelect(stmt->select_source);

            // Verify column count matches
            if (resolved->select_source &&
                resolved->target_column_indexes.size() != resolved->select_source->select_list.size()) {
                error(stmt->span, "INSERT column count doesn't match SELECT column count");
            }
            break;

        case InsertStmt::Source::DEFAULT:
            resolved->source = ResolvedInsertStmt::Source::DEFAULT;
            break;

        case InsertStmt::Source::VALUES:
            resolved->source = ResolvedInsertStmt::Source::VALUES;

            // Analyze value rows
            for (const auto& row : stmt->values_rows) {
                if (row.size() != resolved->target_column_indexes.size()) {
                    error(stmt->span, "VALUES row has wrong number of columns");
                    continue;
                }

                std::vector<ResolvedExpression*> resolved_row;
                for (auto* expr : row) {
                    resolved_row.push_back(analyzeExpression(expr));
                }
                resolved->values_rows.push_back(std::move(resolved_row));
            }
            break;
    }

    // Analyze ON CONFLICT if present
    if (stmt->on_conflict) {
        auto* on_conflict = new ResolvedInsertStmt::OnConflict();
        on_conflict->action = stmt->on_conflict->action;

        // Resolve conflict columns
        for (auto col_name : stmt->on_conflict->columns) {
            bool found = false;
            for (uint32_t i = 0; i < table_ref->columns.size(); ++i) {
                if (table_ref->columns[i].name == col_name) {
                    on_conflict->conflict_columns.push_back(i);
                    found = true;
                    break;
                }
            }
            if (!found) {
                error(stmt->span, "ON CONFLICT column not found: " + std::string(getString(col_name)));
            }
        }

        // Analyze update assignments for DO UPDATE
        if (stmt->on_conflict->action == ConflictAction::UPDATE) {
            for (const auto& assign : stmt->on_conflict->set_items) {
                bool found = false;
                for (uint32_t i = 0; i < table_ref->columns.size(); ++i) {
                    if (table_ref->columns[i].name == assign.first) {
                        on_conflict->update_assignments.push_back(
                            std::make_pair(i, analyzeExpression(assign.second)));
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    error(stmt->span, "UPDATE column not found: " + std::string(getString(assign.first)));
                }
            }

            if (stmt->on_conflict->where_action) {
                on_conflict->where = analyzeExpression(stmt->on_conflict->where_action);
            }
        }

        resolved->on_conflict.reset(on_conflict);
    }

    // Analyze RETURNING clause
    analyzeReturningClause(stmt->returning, resolved->returning);

    popScope();
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeUpdate(UpdateStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedUpdateStmt>();
    resolved->span = stmt->span;

    // Resolve target table
    auto table_ref = resolveTable(stmt->table_path, stmt->span);
    if (!table_ref) {
        return nullptr;
    }
    resolved->target_table = *table_ref;

    // Add table to scope
    pushScope();
    ResolutionScope::TableEntry entry;
    entry.table_uuid = table_ref->table_uuid;
    entry.columns = table_ref->columns;
    entry.alias = stmt->alias;
    currentScope().addTable(entry);

    // Analyze FROM clause if present (for UPDATE ... FROM ...)
    if (stmt->from) {
        auto* from_ref = analyzeTableRef(stmt->from);
        if (from_ref) {
            resolved->from_tables.push_back(from_ref);
        }
    }

    // Analyze SET assignments (set_items is the field name in UpdateStmt)
    for (const auto& assign : stmt->set_items) {
        bool found = false;
        for (uint32_t i = 0; i < table_ref->columns.size(); ++i) {
            if (table_ref->columns[i].name == assign.first) {
                auto* expr = analyzeExpression(assign.second);
                resolved->assignments.push_back(std::make_pair(i, expr));
                found = true;
                break;
            }
        }
        if (!found) {
            error(stmt->span, "SET column not found: " + std::string(getString(assign.first)));
        }
    }

    // Analyze WHERE clause
    if (stmt->where) {
        resolved->where = analyzeExpression(stmt->where);
    }

    // Analyze RETURNING clause
    analyzeReturningClause(stmt->returning, resolved->returning);

    popScope();
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeDelete(DeleteStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedDeleteStmt>();
    resolved->span = stmt->span;

    // Resolve target table
    auto table_ref = resolveTable(stmt->table_path, stmt->span);
    if (!table_ref) {
        return nullptr;
    }
    resolved->target_table = *table_ref;

    // Add table to scope
    pushScope();
    ResolutionScope::TableEntry entry;
    entry.table_uuid = table_ref->table_uuid;
    entry.columns = table_ref->columns;
    entry.alias = stmt->alias;
    currentScope().addTable(entry);

    // Analyze USING clause if present (using_clause is the field name in DeleteStmt)
    if (stmt->using_clause) {
        auto* using_ref = analyzeTableRef(stmt->using_clause);
        if (using_ref) {
            resolved->using_tables.push_back(using_ref);
        }
    }

    // Analyze WHERE clause
    if (stmt->where) {
        resolved->where = analyzeExpression(stmt->where);
    }

    // Analyze RETURNING clause
    analyzeReturningClause(stmt->returning, resolved->returning);

    popScope();
    return resolved;
}

ResolvedStatement* SemanticAnalyzerV2::analyzeCopy(CopyStmt* stmt) {
    if (!stmt) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCopyStmt>();
    resolved->span = stmt->span;
    resolved->direction = (stmt->direction == CopyStmt::Direction::FROM)
        ? ResolvedCopyStmt::Direction::FROM
        : ResolvedCopyStmt::Direction::TO;
    resolved->target_is_stdin = stmt->target_is_stdin;
    resolved->target_is_stdout = stmt->target_is_stdout;
    resolved->target = stmt->target;

    if (stmt->query) {
        if (stmt->direction == CopyStmt::Direction::FROM) {
            error(stmt->span, "COPY (SELECT ...) only supports TO");
        }
        if (!stmt->columns.empty()) {
            error(stmt->span, "COPY (SELECT ...) cannot specify a column list");
        }

        resolved->query = analyzeSelect(stmt->query);
        if (!resolved->query) {
            return nullptr;
        }
        resolved->has_table = false;
        resolved->table_path = StringPool::INVALID_ID;
    } else {
        resolved->has_table = true;
        resolved->table_path = internString(schemaPathToString(stmt->table_path, string_pool_));

        // Resolve target table
        auto table_ref = resolveTable(stmt->table_path, stmt->span);
        if (!table_ref) {
            return nullptr;
        }
        resolved->target_table = *table_ref;

        // Resolve target columns
        if (!stmt->columns.empty()) {
            for (auto col_name : stmt->columns) {
                bool found = false;
                for (uint32_t i = 0; i < table_ref->columns.size(); ++i) {
                    if (table_ref->columns[i].name == col_name) {
                        resolved->target_column_indexes.push_back(i);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    error(stmt->span, "Column not found: " + std::string(getString(col_name)));
                }
            }
        }
    }

    if (!resolved->target_is_stdin &&
        !resolved->target_is_stdout &&
        resolved->target == StringPool::INVALID_ID) {
        error(stmt->span, "COPY requires a target file or STDIN/STDOUT");
    }

    {
        auto to_upper = [](std::string_view input) {
            std::string out;
            out.reserve(input.size());
            for (char c : input) {
                out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            }
            return out;
        };

        ResolvedCopyOptions opts;
        if (stmt->options.format_set) {
            switch (stmt->options.format) {
                case CopyOptions::Format::CSV:
                    opts.format = ResolvedCopyOptions::Format::CSV;
                    break;
                case CopyOptions::Format::TEXT:
                    opts.format = ResolvedCopyOptions::Format::TEXT;
                    break;
                case CopyOptions::Format::BINARY:
                    opts.format = ResolvedCopyOptions::Format::BINARY;
                    break;
            }
        }

        if (opts.format == ResolvedCopyOptions::Format::CSV) {
            opts.delimiter = ',';
            opts.null_string.clear();
        } else {
            opts.delimiter = '\t';
            opts.null_string = "\\N";
        }

        if (stmt->options.delimiter_set) {
            auto delim = getString(stmt->options.delimiter);
            if (delim.size() != 1) {
                error(stmt->span, "COPY DELIMITER must be a single character");
            } else {
                opts.delimiter = delim[0];
            }
        }

        if (stmt->options.null_set) {
            opts.null_string = std::string(getString(stmt->options.null_string));
        }

        if (stmt->options.header_set) {
            opts.header = stmt->options.header;
        }

        const bool quote_set = stmt->options.quote_set;
        const bool escape_set = stmt->options.escape_set;

        if (quote_set) {
            auto quote = getString(stmt->options.quote);
            if (quote.size() != 1) {
                error(stmt->span, "COPY QUOTE must be a single character");
            } else {
                opts.quote = quote[0];
            }
        }

        if (escape_set) {
            auto escape = getString(stmt->options.escape);
            if (escape.size() != 1) {
                error(stmt->span, "COPY ESCAPE must be a single character");
            } else {
                opts.escape = escape[0];
            }
        }

        if (opts.format == ResolvedCopyOptions::Format::CSV && !escape_set) {
            opts.escape = opts.quote;
        }

        if (stmt->options.encoding_set) {
            opts.encoding = std::string(getString(stmt->options.encoding));
            auto enc_upper = to_upper(opts.encoding);
            if (!enc_upper.empty() && enc_upper != "UTF8" && enc_upper != "UTF-8") {
                error(stmt->span, "COPY ENCODING is not supported");
            }
        }

        if (opts.format == ResolvedCopyOptions::Format::BINARY) {
            error(stmt->span, "COPY FORMAT BINARY is not supported");
        }

        resolved->options = std::move(opts);
    }

    return resolved;
}

// =============================================================================
// Expression Analysis
// =============================================================================

ResolvedExpression* SemanticAnalyzerV2::analyzeExpression(Expression* expr) {
    if (!expr) {
        return nullptr;
    }

    switch (expr->kind()) {
        case ASTKind::LiteralExpr:
            return analyzeLiteral(static_cast<LiteralExpr*>(expr));
        case ASTKind::ColumnRefExpr:
            return analyzeColumnRef(static_cast<ColumnRefExpr*>(expr));
        case ASTKind::BinaryExpr:
            return analyzeBinaryExpr(static_cast<BinaryExpr*>(expr));
        case ASTKind::UnaryExpr:
            return analyzeUnaryExpr(static_cast<UnaryExpr*>(expr));
        case ASTKind::FunctionCallExpr:
            return analyzeFunctionCall(static_cast<FunctionCallExpr*>(expr));
        case ASTKind::CastExpr:
            return analyzeCast(static_cast<CastExpr*>(expr));
        case ASTKind::ExtractExpr:
            return analyzeExtract(static_cast<ExtractExpr*>(expr));
        case ASTKind::AlterElementExpr:
            return analyzeAlterElement(static_cast<AlterElementExpr*>(expr));
        case ASTKind::CaseExpr:
            return analyzeCase(static_cast<CaseExpr*>(expr));
        case ASTKind::SubqueryExpr:
            return analyzeSubquery(static_cast<SubqueryExpr*>(expr));
        case ASTKind::ExistsExpr:
            return analyzeExists(static_cast<ExistsExpr*>(expr));
        case ASTKind::InExpr:
            return analyzeIn(static_cast<InExpr*>(expr));
        case ASTKind::BetweenExpr:
            return analyzeBetween(static_cast<BetweenExpr*>(expr));
        case ASTKind::LikeExpr:
            return analyzeLike(static_cast<LikeExpr*>(expr));
        case ASTKind::IsNullExpr:
            return analyzeIsNull(static_cast<IsNullExpr*>(expr));
        case ASTKind::ArrayExpr:
            return analyzeArray(static_cast<ArrayExpr*>(expr));
        default:
            error(expr->span, "Unknown expression type");
            return nullptr;
    }
}

ResolvedExpression* SemanticAnalyzerV2::analyzeLiteral(LiteralExpr* expr) {
    auto* resolved = arena_.create<ResolvedLiteral>();
    resolved->span = expr->span;
    resolved->literal_type = expr->literal_type;  // Copy the literal type

    switch (expr->literal_type) {
        case LiteralType::INTEGER:
            resolved->type.data_type = DataType::INT64;
            resolved->type.is_nullable = false;
            resolved->int_value = expr->int_value;
            break;

        case LiteralType::FLOAT:
            resolved->type.data_type = DataType::FLOAT64;
            resolved->type.is_nullable = false;
            resolved->float_value = expr->float_value;
            break;

        case LiteralType::STRING:
            resolved->type.data_type = DataType::VARCHAR;
            resolved->type.is_nullable = false;
            resolved->string_value = expr->string_value;
            break;

        case LiteralType::BLOB:
            resolved->type.data_type = DataType::BLOB;
            resolved->type.is_nullable = false;
            resolved->string_value = expr->string_value;
            break;

        case LiteralType::BOOLEAN:
            resolved->type.data_type = DataType::BOOLEAN;
            resolved->type.is_nullable = false;
            resolved->bool_value = expr->bool_value;
            break;

        case LiteralType::NULL_VALUE:
            resolved->type.data_type = DataType::UNKNOWN;  // NULL takes type from context
            resolved->type.is_nullable = true;
            resolved->is_null = true;
            break;

        case LiteralType::DEFAULT:
            resolved->type.data_type = DataType::UNKNOWN;  // DEFAULT type determined by column
            resolved->type.is_nullable = true;
            resolved->is_default = true;
            break;
    }

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeColumnRef(ColumnRefExpr* expr) {
    StringPool::StringId table_alias = StringPool::INVALID_ID;
    if (expr->column.has_table_qualifier && !expr->column.table_path.components.empty()) {
        // Use last component of table path as alias
        table_alias = expr->column.table_path.components.back();
    }

    auto resolved_col = resolveColumn(table_alias, expr->column.column_name, expr->span);
    if (!resolved_col) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedColumnRefExpr>();
    resolved->span = expr->span;
    resolved->column = *resolved_col;
    resolved->type.data_type = resolved_col->data_type;
    resolved->type.is_nullable = resolved_col->is_nullable;

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeBinaryExpr(BinaryExpr* expr) {
    auto* left = analyzeExpression(expr->left);
    auto* right = analyzeExpression(expr->right);

    if (!left || !right) {
        return nullptr;
    }

    auto common_type = getCommonType(left->type, right->type, expr->op);
    if (!common_type) {
        if (expr->op == BinaryOp::ADD || expr->op == BinaryOp::SUB ||
            expr->op == BinaryOp::MUL || expr->op == BinaryOp::DIV ||
            expr->op == BinaryOp::MOD) {
            warning(expr->span,
                    "Incompatible types for arithmetic operator; deferring type checking");
            auto* resolved = arena_.create<ResolvedBinaryExpr>();
            resolved->span = expr->span;
            resolved->op = expr->op;
            resolved->left = left;
            resolved->right = right;
            resolved->type.data_type = DataType::UNKNOWN;
            resolved->type.is_nullable = left->type.is_nullable || right->type.is_nullable ||
                                         left->type.data_type == DataType::UNKNOWN ||
                                         right->type.data_type == DataType::UNKNOWN;
            return resolved;
        }
        error(expr->span, "Incompatible types for binary operator");
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedBinaryExpr>();
    resolved->span = expr->span;
    resolved->op = expr->op;
    resolved->left = left;
    resolved->right = right;
    resolved->type = *common_type;

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeUnaryExpr(UnaryExpr* expr) {
    auto* operand = analyzeExpression(expr->operand);
    if (!operand) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedUnaryExpr>();
    resolved->span = expr->span;
    resolved->op = expr->op;
    resolved->operand = operand;

    switch (expr->op) {
        case UnaryOp::NEGATE:
            if (!operand->type.isNumeric()) {
                error(expr->span, "NEGATE operator requires numeric operand");
                return nullptr;
            }
            resolved->type = operand->type;
            break;

        case UnaryOp::NOT:
            if (!operand->type.isBoolean()) {
                error(expr->span, "NOT operator requires boolean operand");
                return nullptr;
            }
            resolved->type.data_type = DataType::BOOLEAN;
            resolved->type.is_nullable = operand->type.is_nullable;
            break;

        case UnaryOp::BIT_NOT:
            if (!operand->type.isNumeric()) {
                error(expr->span, "Bitwise NOT operator requires numeric operand");
                return nullptr;
            }
            resolved->type = operand->type;
            break;

        case UnaryOp::IS_NULL:
        case UnaryOp::IS_NOT_NULL:
            resolved->type.data_type = DataType::BOOLEAN;
            resolved->type.is_nullable = false;
            break;
    }

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeFunctionCall(FunctionCallExpr* expr) {
    // Analyze arguments first
    std::vector<ResolvedType> arg_types;
    std::vector<ResolvedExpression*> resolved_args;

    for (auto* arg : expr->arguments) {
        auto* resolved_arg = analyzeExpression(arg);
        if (!resolved_arg) {
            return nullptr;
        }
        arg_types.push_back(resolved_arg->type);
        resolved_args.push_back(resolved_arg);
    }

    // Resolve function
    auto func_ref = resolveFunction(expr->function_path, arg_types, expr->span);
    if (!func_ref) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedFunctionCall>();
    resolved->span = expr->span;
    resolved->function = *func_ref;
    resolved->arguments = std::move(resolved_args);
    resolved->distinct = expr->distinct;

    // Analyze FILTER clause if present
    if (expr->filter) {
        resolved->filter = analyzeExpression(expr->filter);
    }

    // Set result type from function
    if (func_ref->return_type) {
        resolved->type = *func_ref->return_type;
    }

    // Track aggregate usage
    if (func_ref->is_aggregate) {
        has_aggregates_ = true;
        if (in_aggregate_) {
            error(expr->span, "Nested aggregate functions are not allowed");
            return nullptr;
        }
    }

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeCast(CastExpr* expr) {
    auto* operand = analyzeExpression(expr->expr);
    if (!operand) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedCast>();
    resolved->span = expr->span;
    resolved->expr = operand;
    resolved->target_type = resolveTypeName(expr->target_type);
    resolved->type = resolved->target_type;
    resolved->format = resolveCastFormat(expr);
    resolved->implicit = false;

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeExtract(ExtractExpr* expr) {
    auto* source = analyzeExpression(expr->source);
    if (!source) {
        return nullptr;
    }

    ExtractField field = ExtractField::VALUE;
    ResolvedElementSelector selector;

    auto makeStringLiteral = [&](StringPool::StringId id) -> ResolvedExpression* {
        auto* literal = arena_.create<ResolvedLiteral>();
        literal->span = expr->span;
        literal->literal_type = LiteralType::STRING;
        literal->string_value = id;
        literal->type.data_type = DataType::VARCHAR;
        literal->type.is_nullable = false;
        return literal;
    };

    if (expr->selector.kind == ElementSelector::Kind::STRING_LITERAL) {
        field = ExtractField::PATH;
        selector.args.push_back(makeStringLiteral(expr->selector.string_literal));
    } else if (expr->selector.kind == ElementSelector::Kind::INTEGER_EXPR) {
        field = ExtractField::ELEMENT;
        auto* arg = analyzeExpression(expr->selector.expr);
        if (!arg) {
            return nullptr;
        }
        selector.args.push_back(arg);
    } else {
        selector.field_name = expr->selector.identifier;
        std::string_view field_name = getString(expr->selector.identifier);
        auto resolved = resolveExtractFieldName(field_name);
        if (!resolved.has_value()) {
            if (!expr->selector.args.empty()) {
                error(expr->span, "Unknown EXTRACT element: " + std::string(field_name));
                return nullptr;
            }
            field = ExtractField::FIELD;
            selector.args.push_back(makeStringLiteral(expr->selector.identifier));
        } else {
            field = resolved.value();
            for (auto* arg_expr : expr->selector.args) {
                auto* arg = analyzeExpression(arg_expr);
                if (!arg) {
                    return nullptr;
                }
                selector.args.push_back(arg);
            }
        }
    }

    ElementArgSpec arg_spec = extractFieldArgSpec(field);
    if (selector.args.size() < arg_spec.min_args ||
        selector.args.size() > arg_spec.max_args) {
        error(expr->span, "Invalid argument count for EXTRACT(" +
                              std::string(extractFieldToString(field)) + ")");
        return nullptr;
    }

    ElementInfo info = resolveElementInfo(source->type, field);
    if (!info.allowed) {
        error(expr->span, "Element '" + std::string(extractFieldToString(field)) +
                              "' not valid for type " +
                              std::string(core::TypeSystem::getTypeName(source->type.data_type)));
        return nullptr;
    }

    selector.field_id = static_cast<uint8_t>(field);

    auto* resolved = arena_.create<ResolvedExtractExpr>();
    resolved->span = expr->span;
    resolved->selector = selector;
    resolved->source = source;
    resolved->type = info.type;
    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeAlterElement(AlterElementExpr* expr) {
    auto* source = analyzeExpression(expr->source);
    if (!source) {
        return nullptr;
    }

    auto* new_value = analyzeExpression(expr->new_value);
    if (!new_value) {
        return nullptr;
    }

    ExtractField field = ExtractField::VALUE;
    ResolvedElementSelector selector;

    auto makeStringLiteral = [&](StringPool::StringId id) -> ResolvedExpression* {
        auto* literal = arena_.create<ResolvedLiteral>();
        literal->span = expr->span;
        literal->literal_type = LiteralType::STRING;
        literal->string_value = id;
        literal->type.data_type = DataType::VARCHAR;
        literal->type.is_nullable = false;
        return literal;
    };

    if (expr->selector.kind == ElementSelector::Kind::STRING_LITERAL) {
        field = ExtractField::PATH;
        selector.args.push_back(makeStringLiteral(expr->selector.string_literal));
    } else if (expr->selector.kind == ElementSelector::Kind::INTEGER_EXPR) {
        field = ExtractField::ELEMENT;
        auto* arg = analyzeExpression(expr->selector.expr);
        if (!arg) {
            return nullptr;
        }
        selector.args.push_back(arg);
    } else {
        selector.field_name = expr->selector.identifier;
        std::string_view field_name = getString(expr->selector.identifier);
        auto resolved = resolveExtractFieldName(field_name);
        if (!resolved.has_value()) {
            if (!expr->selector.args.empty()) {
                error(expr->span, "Unknown ALTER_ELEMENT element: " + std::string(field_name));
                return nullptr;
            }
            field = ExtractField::FIELD;
            selector.args.push_back(makeStringLiteral(expr->selector.identifier));
        } else {
            field = resolved.value();
            for (auto* arg_expr : expr->selector.args) {
                auto* arg = analyzeExpression(arg_expr);
                if (!arg) {
                    return nullptr;
                }
                selector.args.push_back(arg);
            }
        }
    }

    ElementArgSpec arg_spec = extractFieldArgSpec(field);
    if (selector.args.size() < arg_spec.min_args ||
        selector.args.size() > arg_spec.max_args) {
        error(expr->span, "Invalid argument count for ALTER_ELEMENT(" +
                              std::string(extractFieldToString(field)) + ")");
        return nullptr;
    }

    ElementInfo info = resolveElementInfo(source->type, field);
    if (!info.allowed) {
        error(expr->span, "Element '" + std::string(extractFieldToString(field)) +
                              "' not valid for type " +
                              std::string(core::TypeSystem::getTypeName(source->type.data_type)));
        return nullptr;
    }
    if (!info.writable) {
        error(expr->span, "Element '" + std::string(extractFieldToString(field)) +
                              "' is read-only");
        return nullptr;
    }

    if (info.type.data_type != DataType::UNKNOWN &&
        info.type.data_type != DataType::VARIANT) {
        new_value = insertImplicitCast(new_value, info.type);
    }

    selector.field_id = static_cast<uint8_t>(field);

    auto* resolved = arena_.create<ResolvedAlterElementExpr>();
    resolved->span = expr->span;
    resolved->selector = selector;
    resolved->source = source;
    resolved->new_value = new_value;
    resolved->type = source->type;
    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeCase(CaseExpr* expr) {
    auto* resolved = arena_.create<ResolvedCase>();
    resolved->span = expr->span;

    if (expr->when_clauses.empty()) {
        error(expr->span, "CASE expression must contain at least one WHEN clause");
        return nullptr;
    }

    // Analyze operand for simple CASE
    if (expr->operand) {
        resolved->operand = analyzeExpression(expr->operand);
        if (!resolved->operand) {
            return nullptr;
        }
    }

    // Track result type from WHEN clauses
    ResolvedType result_type;
    bool first_result = true;

    for (const auto& when : expr->when_clauses) {
        ResolvedCase::WhenClause resolved_when;

        resolved_when.when_expr = analyzeExpression(when.when_expr);
        if (!resolved_when.when_expr) {
            return nullptr;
        }

        resolved_when.then_expr = analyzeExpression(when.then_expr);
        if (!resolved_when.then_expr) {
            return nullptr;
        }

        // Type check WHEN condition
        if (expr->operand) {
            // Simple CASE: WHEN value must match operand type
            if (!resolved_when.when_expr->type.isComparableTo(resolved->operand->type)) {
                error(when.when_expr->span, "WHEN value type doesn't match CASE operand");
                return nullptr;
            }
        } else {
            // Searched CASE: WHEN must be boolean
            if (!resolved_when.when_expr->type.isBoolean()) {
                error(when.when_expr->span, "WHEN condition must be boolean");
                return nullptr;
            }
        }

        // Track common result type
        if (first_result) {
            result_type = resolved_when.then_expr->type;
            first_result = false;
        } else {
            if (!result_type.isComparableTo(resolved_when.then_expr->type)) {
                error(when.then_expr->span, "CASE result types are incompatible");
                return nullptr;
            }
        }

        resolved->when_clauses.push_back(resolved_when);
    }

    // Analyze ELSE
    if (expr->else_expr) {
        resolved->else_expr = analyzeExpression(expr->else_expr);
        if (!resolved->else_expr) {
            return nullptr;
        }
        if (!first_result && !result_type.isComparableTo(resolved->else_expr->type)) {
            error(expr->else_expr->span, "ELSE type is incompatible with WHEN results");
            return nullptr;
        }
    } else {
        result_type.is_nullable = true;  // No ELSE means NULL is possible
    }

    resolved->type = result_type;
    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeSubquery(SubqueryExpr* expr) {
    ++subquery_depth_;
    pushScope();

    auto* subselect = analyzeSelect(expr->subquery);

    popScope();
    --subquery_depth_;

    if (!subselect) {
        return nullptr;
    }

    // Scalar subquery must return single column
    if (subselect->select_list.size() != 1) {
        error(expr->span, "Scalar subquery must return exactly one column");
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedSubqueryExpr>();
    resolved->span = expr->span;
    resolved->subquery = subselect;
    resolved->type = subselect->select_list[0].type;
    resolved->type.is_nullable = true;  // Subquery may return 0 rows

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeExists(ExistsExpr* expr) {
    ++subquery_depth_;
    pushScope();

    auto* subselect = analyzeSelect(expr->subquery);

    popScope();
    --subquery_depth_;

    if (!subselect) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedExistsExpr>();
    resolved->span = expr->span;
    resolved->subquery = subselect;
    resolved->negated = expr->negated;
    resolved->type.data_type = DataType::BOOLEAN;
    resolved->type.is_nullable = false;

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeIn(InExpr* expr) {
    auto* left = analyzeExpression(expr->expr);
    if (!left) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedInExpr>();
    resolved->span = expr->span;
    resolved->expr = left;
    resolved->negated = expr->negated;
    resolved->type.data_type = DataType::BOOLEAN;
    resolved->type.is_nullable = false;

    if (expr->has_subquery) {
        ++subquery_depth_;
        pushScope();

        auto* subselect = analyzeSelect(expr->subquery);

        popScope();
        --subquery_depth_;

        if (!subselect) {
            return nullptr;
        }

        // IN subquery must return single column
        if (subselect->select_list.size() != 1) {
            error(expr->span, "IN subquery must return exactly one column");
            return nullptr;
        }

        if (!left->type.isComparableTo(subselect->select_list[0].type)) {
            error(expr->span, "IN expression and subquery types are incompatible");
            return nullptr;
        }

        resolved->subquery = subselect;
        resolved->has_subquery = true;
    } else {
        for (auto* val : expr->values) {
            auto* resolved_val = analyzeExpression(val);
            if (!resolved_val) {
                return nullptr;
            }

            if (!left->type.isComparableTo(resolved_val->type)) {
                error(val->span, "IN list value type is incompatible with expression");
                return nullptr;
            }

            resolved->values.push_back(resolved_val);
        }
    }

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeBetween(BetweenExpr* expr) {
    auto* operand = analyzeExpression(expr->expr);
    auto* low = analyzeExpression(expr->low);
    auto* high = analyzeExpression(expr->high);

    if (!operand || !low || !high) {
        return nullptr;
    }

    if (!operand->type.isComparableTo(low->type) || !operand->type.isComparableTo(high->type)) {
        error(expr->span, "BETWEEN operand types are incompatible");
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedBetweenExpr>();
    resolved->span = expr->span;
    resolved->expr = operand;
    resolved->negated = expr->negated;
    resolved->symmetric = expr->symmetric;
    resolved->low = low;
    resolved->high = high;
    resolved->type.data_type = DataType::BOOLEAN;
    resolved->type.is_nullable = operand->type.is_nullable || low->type.is_nullable || high->type.is_nullable;

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeLike(LikeExpr* expr) {
    auto* operand = analyzeExpression(expr->expr);
    auto* pattern = analyzeExpression(expr->pattern);

    if (!operand || !pattern) {
        return nullptr;
    }

    if (!operand->type.isString()) {
        error(expr->expr->span, "LIKE operand must be a string type");
        return nullptr;
    }

    if (!pattern->type.isString()) {
        error(expr->pattern->span, "LIKE pattern must be a string type");
        return nullptr;
    }

    auto make_string_literal = [&](std::string_view value) -> ResolvedLiteral* {
        auto* literal = arena_.create<ResolvedLiteral>();
        literal->literal_type = LiteralType::STRING;
        literal->string_value = string_pool_.intern(value);
        literal->type.data_type = DataType::VARCHAR;
        literal->type.is_nullable = false;
        literal->span = expr->span;
        return literal;
    };

    auto make_concat = [&](ResolvedExpression* left, ResolvedExpression* right) -> ResolvedBinaryExpr* {
        auto* concat = arena_.create<ResolvedBinaryExpr>();
        concat->op = BinaryOp::CONCAT;
        concat->left = left;
        concat->right = right;
        concat->span = expr->span;
        concat->type.data_type = DataType::VARCHAR;
        concat->type.is_nullable = left->type.is_nullable || right->type.is_nullable;
        return concat;
    };

    auto* resolved = arena_.create<ResolvedLikeExpr>();
    resolved->span = expr->span;
    resolved->expr = operand;
    resolved->negated = expr->negated;
    resolved->match_kind = expr->match_kind;
    resolved->case_insensitive =
        (expr->match_kind == LikeMatchKind::ILIKE || expr->match_kind == LikeMatchKind::CONTAINING);
    if (expr->match_kind == LikeMatchKind::LIKE || expr->match_kind == LikeMatchKind::STARTING ||
        expr->match_kind == LikeMatchKind::SIMILAR) {
        resolved->case_insensitive = expr->case_insensitive;
    }

    if ((expr->match_kind == LikeMatchKind::CONTAINING || expr->match_kind == LikeMatchKind::STARTING) &&
        expr->escape) {
        error(expr->escape->span, "ESCAPE is not supported with CONTAINING/STARTING predicates");
        return nullptr;
    }

    ResolvedExpression* pattern_expr = pattern;
    if (expr->match_kind == LikeMatchKind::CONTAINING) {
        auto* prefix = make_string_literal("%");
        auto* suffix = make_string_literal("%");
        pattern_expr = make_concat(make_concat(prefix, pattern), suffix);
    } else if (expr->match_kind == LikeMatchKind::STARTING) {
        auto* suffix = make_string_literal("%");
        pattern_expr = make_concat(pattern, suffix);
    }

    resolved->pattern = pattern_expr;
    resolved->type.data_type = DataType::BOOLEAN;
    resolved->type.is_nullable = operand->type.is_nullable || pattern->type.is_nullable;

    if (expr->escape) {
        resolved->escape = analyzeExpression(expr->escape);
        if (!resolved->escape) {
            return nullptr;
        }
        if (!resolved->escape->type.isString()) {
            error(expr->escape->span, "LIKE ESCAPE must be a string type");
            return nullptr;
        }
    }

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeIsNull(IsNullExpr* expr) {
    auto* operand = analyzeExpression(expr->expr);
    if (!operand) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedIsNullExpr>();
    resolved->span = expr->span;
    resolved->expr = operand;
    resolved->negated = expr->negated;
    resolved->type.data_type = DataType::BOOLEAN;
    resolved->type.is_nullable = false;

    return resolved;
}

ResolvedExpression* SemanticAnalyzerV2::analyzeArray(ArrayExpr* expr) {
    auto* resolved = arena_.create<ResolvedArrayExpr>();
    resolved->span = expr->span;

    if (expr->has_subquery) {
        ++subquery_depth_;
        pushScope();

        auto* subselect = analyzeSelect(expr->subquery);

        popScope();
        --subquery_depth_;

        if (!subselect) {
            return nullptr;
        }

        if (subselect->select_list.size() != 1) {
            error(expr->span, "ARRAY subquery must return exactly one column");
            return nullptr;
        }

        resolved->subquery = subselect;
        resolved->has_subquery = true;
        resolved->type = subselect->select_list[0].type;
        resolved->type.is_array = true;
    } else {
        // Determine element type from first element
        ResolvedType element_type;
        bool first = true;

        for (auto* elem : expr->elements) {
            auto* resolved_elem = analyzeExpression(elem);
            if (!resolved_elem) {
                return nullptr;
            }

            if (first) {
                element_type = resolved_elem->type;
                first = false;
            } else {
                if (!element_type.isComparableTo(resolved_elem->type)) {
                    error(elem->span, "Array element types must be compatible");
                    return nullptr;
                }
            }

            resolved->elements.push_back(resolved_elem);
        }

        resolved->type = element_type;
        resolved->type.is_array = true;
    }

    return resolved;
}

// =============================================================================
// Clause Analysis
// =============================================================================

void SemanticAnalyzerV2::analyzeFromClause(SelectStmt* stmt, ResolvedSelectStmt* resolved) {
    if (!stmt->from) {
        return;  // SELECT without FROM (e.g., SELECT 1+1)
    }

    // Analyze main table reference
    auto* main_ref = analyzeTableRef(stmt->from);
    if (main_ref) {
        resolved->from_tables.push_back(main_ref);

        // Add to scope
        ResolutionScope::TableEntry entry;
        entry.table_uuid = main_ref->table_uuid;
        entry.alias = main_ref->alias;
        entry.columns = main_ref->columns;
        entry.is_cte = (main_ref->object_type == ResolvedTableRef::ObjectType::CTE);
        currentScope().addTable(entry);
    }

    // Analyze joins
    for (auto* join : stmt->joins) {
        auto* resolved_join = analyzeJoin(join);
        if (resolved_join) {
            resolved->joins.push_back(resolved_join);
        }
    }
}

void SemanticAnalyzerV2::analyzeWhereClause(Expression* where, ResolvedExpression*& resolved) {
    if (where) {
        resolved = analyzeExpression(where);
    }
}

void SemanticAnalyzerV2::analyzeGroupByClause(SelectStmt* stmt, ResolvedSelectStmt* resolved) {
    for (auto* expr : stmt->group_by) {
        auto* resolved_expr = analyzeExpression(expr);
        if (resolved_expr) {
            resolved->group_by.push_back(resolved_expr);
        }
    }
}

void SemanticAnalyzerV2::analyzeHavingClause(Expression* having, ResolvedExpression*& resolved) {
    if (having) {
        resolved = analyzeExpression(having);
    }
}

void SemanticAnalyzerV2::analyzeOrderByClause(
    const std::vector<OrderByItem*>& items,
    std::vector<ResolvedOrderByItem*>& resolved)
{
    for (auto* item : items) {
        auto* resolved_item = arena_.create<ResolvedOrderByItem>();
        resolved_item->expr = analyzeExpression(item->expr);
        resolved_item->ascending = item->ascending;
        resolved_item->nulls_first = item->nulls_first;
        resolved_item->nulls_last = item->nulls_last;
        resolved.push_back(resolved_item);
    }
}

void SemanticAnalyzerV2::analyzeSelectList(SelectStmt* stmt, ResolvedSelectStmt* resolved) {
    for (auto* item : stmt->items) {
        ResolvedSelectItem resolved_item;

        switch (item->item_type) {
            case SelectItem::Type::EXPRESSION:
                resolved_item.item_type = ResolvedSelectItem::ItemType::EXPRESSION;
                resolved_item.expr = analyzeExpression(item->expr);
                resolved_item.alias = item->alias;
                resolved_item.has_alias = item->has_alias;
                if (resolved_item.expr) {
                    resolved_item.type = resolved_item.expr->type;
                }
                break;

            case SelectItem::Type::STAR:
                resolved_item.item_type = ResolvedSelectItem::ItemType::STAR;
                // Expand * to all columns in scope
                for (const auto& table : currentScope().tables()) {
                    for (const auto& col : table.columns) {
                        ResolvedSelectItem col_item;
                        col_item.item_type = ResolvedSelectItem::ItemType::EXPRESSION;

                        // Create a column reference expression
                        auto* col_expr = arena_.create<ResolvedColumnRefExpr>();
                        col_expr->column.table_uuid = table.table_uuid;
                        col_expr->column.column_index = col.column_index;
                        col_expr->column.data_type = col.data_type;
                        col_expr->column.is_nullable = col.is_nullable;
                        col_expr->column.column_name = col.name;
                        col_expr->column.table_alias = table.alias;
                        col_expr->type.data_type = col.data_type;
                        col_expr->type.is_nullable = col.is_nullable;

                        col_item.expr = col_expr;
                        col_item.type.data_type = col.data_type;
                        col_item.type.is_nullable = col.is_nullable;
                        resolved->select_list.push_back(col_item);
                    }
                }
                continue;  // Already added items, skip the push at end

            case SelectItem::Type::TABLE_STAR:
                resolved_item.item_type = ResolvedSelectItem::ItemType::TABLE_STAR;
                // Find the table and expand t.* to its columns
                if (!item->table_path.components.empty()) {
                    StringPool::StringId table_name = item->table_path.components.back();
                    const auto* table = currentScope().findTable(table_name);
                    if (table) {
                        resolved_item.table_uuid = table->table_uuid;
                        for (const auto& col : table->columns) {
                            ResolvedSelectItem col_item;
                            col_item.item_type = ResolvedSelectItem::ItemType::EXPRESSION;

                            auto* col_expr = arena_.create<ResolvedColumnRefExpr>();
                            col_expr->column.table_uuid = table->table_uuid;
                            col_expr->column.column_index = col.column_index;
                            col_expr->column.data_type = col.data_type;
                            col_expr->column.is_nullable = col.is_nullable;
                            col_expr->column.column_name = col.name;
                            col_expr->column.table_alias = table->alias;
                            col_expr->type.data_type = col.data_type;
                            col_expr->type.is_nullable = col.is_nullable;

                            col_item.expr = col_expr;
                            col_item.type.data_type = col.data_type;
                            col_item.type.is_nullable = col.is_nullable;
                            resolved->select_list.push_back(col_item);
                        }
                        continue;  // Already added items
                    } else {
                        error(item->span, "Table not found for .*: " + std::string(getString(table_name)));
                    }
                }
                break;
        }

        resolved->select_list.push_back(resolved_item);
    }
}

void SemanticAnalyzerV2::analyzeReturningClause(
    const std::vector<SelectItem*>& returning,
    std::vector<ResolvedSelectItem>& resolved)
{
    for (auto* item : returning) {
        ResolvedSelectItem resolved_item;

        switch (item->item_type) {
            case SelectItem::Type::EXPRESSION:
                resolved_item.item_type = ResolvedSelectItem::ItemType::EXPRESSION;
                resolved_item.expr = analyzeExpression(item->expr);
                resolved_item.alias = item->alias;
                resolved_item.has_alias = item->has_alias;
                if (resolved_item.expr) {
                    resolved_item.type = resolved_item.expr->type;
                }
                break;

            case SelectItem::Type::STAR:
                // RETURNING * expands to all columns of the target table
                resolved_item.item_type = ResolvedSelectItem::ItemType::STAR;
                break;

            case SelectItem::Type::TABLE_STAR:
                resolved_item.item_type = ResolvedSelectItem::ItemType::TABLE_STAR;
                if (!item->table_path.components.empty()) {
                    StringPool::StringId table_name = item->table_path.components.back();
                    const auto* table = currentScope().findTable(table_name);
                    if (table) {
                        resolved_item.table_uuid = table->table_uuid;
                    } else {
                        error(item->span, "Table not found for .*: " + std::string(getString(table_name)));
                    }
                }
                break;
        }

        resolved.push_back(resolved_item);
    }
}

// =============================================================================
// Table Reference Analysis
// =============================================================================

ResolvedTableRef* SemanticAnalyzerV2::analyzeTableRef(TableRefNode* node) {
    if (!node) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedTableRef>();
    resolved->alias = node->alias;
    resolved->has_alias = node->has_alias;

    switch (node->ref_type) {
        case TableRefNode::Type::TABLE: {
            auto table_ref = resolveTable(node->table_path, node->span);
            if (!table_ref) {
                return nullptr;
            }
            *resolved = *table_ref;
            resolved->alias = node->alias;
            resolved->has_alias = node->has_alias;
            if (!resolved->has_alias) {
                // Allow table-qualified references when no alias is provided.
                resolved->alias = resolved->name;
                resolved->has_alias = true;
            }
            break;
        }

        case TableRefNode::Type::SUBQUERY: {
            ++subquery_depth_;
            pushScope();

            auto* subselect = analyzeSelect(static_cast<SelectStmt*>(node->subquery));

            popScope();
            --subquery_depth_;

            if (!subselect) {
                return nullptr;
            }

            resolved->object_type = ResolvedTableRef::ObjectType::SUBQUERY;
            resolved->subquery = subselect;

            // Build column info from subquery result
            for (size_t i = 0; i < subselect->select_list.size(); ++i) {
                const auto& item = subselect->select_list[i];
                ResolvedTableRef::ColumnInfo col;

                // Use alias if available, otherwise generate name
                if (item.has_alias) {
                    col.name = item.alias;
                } else if (!node->column_aliases.empty() && i < node->column_aliases.size()) {
                    col.name = node->column_aliases[i];
                } else {
                    col.name = internString("column" + std::to_string(i + 1));
                }

                col.data_type = item.type.data_type;
                col.is_nullable = item.type.is_nullable;
                col.column_index = static_cast<uint32_t>(i);
                resolved->columns.push_back(col);
            }
            break;
        }

        case TableRefNode::Type::FUNCTION: {
            // Table-valued function
            auto* func = analyzeFunctionCall(node->function);
            if (!func) {
                return nullptr;
            }

            resolved->object_type = ResolvedTableRef::ObjectType::FUNCTION;
            // For table-valued functions, we'd need to look up the function's
            // return table structure. For now, create a placeholder.
            break;
        }

        case TableRefNode::Type::JOIN: {
            // JOIN is typically handled by analyzeJoin, but handle nested case
            error(node->span, "Unexpected JOIN in table reference");
            return nullptr;
        }
    }

    return resolved;
}

ResolvedJoin* SemanticAnalyzerV2::analyzeJoin(JoinNode* node) {
    if (!node) {
        return nullptr;
    }

    auto* resolved = arena_.create<ResolvedJoin>();
    resolved->join_type = node->join_type;

    // Analyze left side
    resolved->left = analyzeTableRef(node->left);
    if (!resolved->left) {
        return nullptr;
    }

    // Add left side columns to scope
    ResolutionScope::TableEntry left_entry;
    left_entry.table_uuid = resolved->left->table_uuid;
    left_entry.alias = resolved->left->alias;
    left_entry.columns = resolved->left->columns;
    currentScope().addTable(left_entry);

    // Analyze right side
    resolved->right = analyzeTableRef(node->right);
    if (!resolved->right) {
        return nullptr;
    }

    // Add right side columns to scope
    ResolutionScope::TableEntry right_entry;
    right_entry.table_uuid = resolved->right->table_uuid;
    right_entry.alias = resolved->right->alias;
    right_entry.columns = resolved->right->columns;
    currentScope().addTable(right_entry);

    // Analyze ON condition
    if (node->on_condition) {
        resolved->on_condition = analyzeExpression(node->on_condition);
        if (resolved->on_condition && !resolved->on_condition->type.isBoolean()) {
            error(node->on_condition->span, "JOIN ON condition must be boolean");
        }
    }

    // Handle USING clause
    if (node->has_using) {
        resolved->has_using = true;
        for (auto col_name : node->using_columns) {
            // Find column in both tables
            const ResolvedTableRef::ColumnInfo* left_col = nullptr;
            const ResolvedTableRef::ColumnInfo* right_col = nullptr;

            for (const auto& col : resolved->left->columns) {
                if (col.name == col_name) {
                    left_col = &col;
                    break;
                }
            }

            for (const auto& col : resolved->right->columns) {
                if (col.name == col_name) {
                    right_col = &col;
                    break;
                }
            }

            if (!left_col || !right_col) {
                error(node->span, "USING column not found in both tables: " + std::string(getString(col_name)));
                continue;
            }

            ResolvedColumnRef resolved_col;
            resolved_col.table_uuid = resolved->left->table_uuid;
            resolved_col.column_index = left_col->column_index;
            resolved_col.data_type = left_col->data_type;
            resolved_col.is_nullable = left_col->is_nullable;
            resolved_col.column_name = col_name;
            resolved->using_columns.push_back(resolved_col);
        }
    }

    return resolved;
}

// =============================================================================
// Type Resolution Helpers
// =============================================================================

ResolvedType SemanticAnalyzerV2::resolveTypeName(const TypeName& type_name) {
    ResolvedType resolved;
    resolved.is_nullable = true;
    resolved.data_type = DataType::UNKNOWN;

    auto apply_domain = [&](const core::DomainInfo& dinfo) {
        resolved.is_domain = true;
        resolved.domain_id = dinfo.domain_id;
        resolved.data_type = dinfo.base_type;
        if (dinfo.base_type == DataType::VARCHAR || dinfo.base_type == DataType::CHAR) {
            resolved.length = static_cast<int32_t>(dinfo.precision);
        } else if (dinfo.base_type == DataType::DECIMAL) {
            resolved.precision = static_cast<int32_t>(dinfo.precision);
            resolved.scale = static_cast<int32_t>(dinfo.scale);
        }
    };

    if (type_name.has_schema_path && !type_name.schema_path.isEmpty()) {
        core::ObjectPath obj_path = buildObjectPath(type_name.schema_path, string_pool_);
        core::CatalogManager::ObjectType resolved_type = core::CatalogManager::ObjectType::UNKNOWN;
        core::ErrorContext ctx;
        core::CatalogManager::ResolveOptions opts;
        opts.allow_search_path = false;
        ID domain_id{};

        Status status = catalog_.resolveObjectPath(
            obj_path,
            core::CatalogManager::ObjectType::DOMAIN,
            opts,
            domain_id,
            resolved_type,
            &ctx);

        if (status == Status::OK) {
            core::DomainInfo dinfo;
            if (catalog_.getDomainById(domain_id, dinfo, &ctx) == Status::OK) {
                apply_domain(dinfo);
                if (current_result_) {
                    current_result_->addDependency(dinfo.domain_id, core::CatalogManager::ObjectType::DOMAIN);
                }
            }
        }
    }

    // Parse type name string to DataType
    if (!resolved.is_domain && type_name.name != StringPool::INVALID_ID) {
        std::string name_str = std::string(string_pool_.get(type_name.name));
        // Convert to lowercase for comparison
        std::transform(name_str.begin(), name_str.end(), name_str.begin(), ::tolower);

        // Map string to DataType
        if (name_str == "int" || name_str == "integer" || name_str == "int32") {
            resolved.data_type = DataType::INT32;
        } else if (name_str == "smallint" || name_str == "int16") {
            resolved.data_type = DataType::INT16;
        } else if (name_str == "bigint" || name_str == "int64") {
            resolved.data_type = DataType::INT64;
        } else if (name_str == "int128") {
            resolved.data_type = DataType::INT128;
        } else if (name_str == "uint128" || name_str == "unsigned int128") {
            resolved.data_type = DataType::UINT128;
        } else if (name_str == "float" || name_str == "real" || name_str == "float32") {
            resolved.data_type = DataType::FLOAT32;
        } else if (name_str == "double" || name_str == "double precision" || name_str == "float64") {
            resolved.data_type = DataType::FLOAT64;
        } else if (name_str == "decimal" || name_str == "numeric") {
            resolved.data_type = DataType::DECIMAL;
        } else if (name_str == "varchar" || name_str == "character varying") {
            resolved.data_type = DataType::VARCHAR;
        } else if (name_str == "char" || name_str == "character") {
            resolved.data_type = DataType::CHAR;
        } else if (name_str == "text") {
            resolved.data_type = DataType::TEXT;
        } else if (name_str == "boolean" || name_str == "bool") {
            resolved.data_type = DataType::BOOLEAN;
        } else if (name_str == "date") {
            resolved.data_type = DataType::DATE;
        } else if (name_str == "time") {
            resolved.data_type = DataType::TIME;
        } else if (name_str == "timestamp") {
            resolved.data_type = DataType::TIMESTAMP;
        } else if (name_str == "interval") {
            resolved.data_type = DataType::INTERVAL;
        } else if (name_str == "blob" || name_str == "bytea") {
            resolved.data_type = DataType::BLOB;
        } else if (name_str == "uuid") {
            resolved.data_type = DataType::UUID;
        } else if (name_str == "json") {
            resolved.data_type = DataType::JSON;
        } else if (name_str == "jsonb") {
            resolved.data_type = DataType::JSONB;
        } else {
            // Try resolving as a domain
            core::ErrorContext ctx;
            core::DomainInfo dinfo;
            // Search current schema then search_path
            bool found = false;
            if (!isZeroUuidLocal(current_schema_) &&
                catalog_.getDomainByName(current_schema_, name_str, dinfo, &ctx) == Status::OK) {
                found = true;
            } else {
                for (const auto& sch : search_path_) {
                    if (catalog_.getDomainByName(sch, name_str, dinfo, &ctx) == Status::OK) {
                        found = true;
                        break;
                    }
                }
            }
            if (found) {
                apply_domain(dinfo);
                if (current_result_) {
                    current_result_->addDependency(dinfo.domain_id, core::CatalogManager::ObjectType::DOMAIN);
                }
            }
        }
    } else {
        if (!resolved.is_domain) {
            resolved.data_type = DataType::UNKNOWN;
        }
    }

    // Copy optional parameters
    if (type_name.precision.has_value()) {
        resolved.precision = type_name.precision.value();
    }
    if (type_name.scale.has_value()) {
        resolved.scale = type_name.scale.value();
    }
    if (type_name.length.has_value()) {
        resolved.length = type_name.length.value();
    }
    resolved.is_array = type_name.is_array;
    if (type_name.array_size.has_value()) {
        resolved.array_size = type_name.array_size.value();
    }
    resolved.with_time_zone = type_name.with_time_zone;

    return resolved;
}

core::CastFormat SemanticAnalyzerV2::resolveCastFormat(const CastExpr* expr) {
    if (!expr || !expr->format.has_value()) {
        return core::CastFormat::DEFAULT;
    }

    std::string fmt = std::string(string_pool_.get(expr->format.value()));
    std::transform(fmt.begin(), fmt.end(), fmt.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (fmt == "hex" || fmt == "hexadecimal") {
        return core::CastFormat::HEX;
    }
    if (fmt == "base64") {
        return core::CastFormat::BASE64;
    }
    if (fmt == "escape") {
        return core::CastFormat::ESCAPE;
    }

    error(expr->span, "Unknown CAST USING format: " + fmt);
    return core::CastFormat::DEFAULT;
}

DataType SemanticAnalyzerV2::mapToDataType(DataType ast_type, int32_t /*precision*/, int32_t /*scale*/) {
    return ast_type;
}

// =============================================================================
// Column Definition Analysis
// =============================================================================

ResolvedColumnDef SemanticAnalyzerV2::analyzeColumnDef(ColumnDef* def) {
    ResolvedColumnDef resolved;

    if (!def) {
        return resolved;
    }

    resolved.name = def->name;
    resolved.type = resolveTypeName(def->type);
    resolved.is_nullable = true;  // Default to nullable

    // Process column constraints
    for (const auto& constraint : def->constraints) {
        switch (constraint.type) {
            case ConstraintType::NOT_NULL:
                resolved.is_nullable = false;
                break;

            case ConstraintType::NULL_ALLOWED:
                resolved.is_nullable = true;
                break;

            case ConstraintType::PRIMARY_KEY:
                resolved.is_primary_key = true;
                resolved.is_nullable = false;  // PK implies NOT NULL
                break;

            case ConstraintType::UNIQUE:
                resolved.is_unique = true;
                break;

            case ConstraintType::DEFAULT:
                if (constraint.default_expr) {
                    resolved.default_value = analyzeExpression(constraint.default_expr);
                }
                break;

            case ConstraintType::CHECK:
                if (constraint.check_expr) {
                    // Set up a temporary scope with the column being defined
                    // so CHECK expressions can reference it
                    pushScope();

                    // Create a pseudo-table entry for the column
                    ResolutionScope::TableEntry col_entry;
                    col_entry.alias = StringPool::INVALID_ID;  // No table qualifier needed
                    col_entry.table_uuid = ID{};

                    // Add the column being defined
                    ResolvedTableRef::ColumnInfo col_info;
                    col_info.name = def->name;
                    col_info.data_type = resolved.type.data_type;
                    col_info.is_nullable = resolved.is_nullable;
                    col_info.column_index = 0;
                    col_entry.columns.push_back(col_info);

                    currentScope().addTable(col_entry);

                    resolved.check_expr = analyzeExpression(constraint.check_expr);

                    popScope();
                }
                break;

            case ConstraintType::REFERENCES:
                resolved.has_fk = true;
                // Would need to resolve the referenced table
                if (!constraint.ref_table.components.empty()) {
                    auto ref_table = resolveTable(constraint.ref_table, SourceSpan{}, false);
                    if (ref_table) {
                        resolved.fk_table_uuid = ref_table->table_uuid;
                        // Find referenced column (use first column from ref_columns vector)
                        if (!constraint.ref_columns.empty()) {
                            auto ref_col = constraint.ref_columns[0];
                            for (uint32_t i = 0; i < ref_table->columns.size(); ++i) {
                                if (ref_table->columns[i].name == ref_col) {
                                    resolved.fk_column_index = i;
                                    break;
                                }
                            }
                        }
                    }
                }
                resolved.on_delete = constraint.on_delete;
                resolved.on_update = constraint.on_update;
                break;

            case ConstraintType::GENERATED:
                // Generated column - would need special handling
                break;

            case ConstraintType::COLLATE:
                // Collation constraint - would need special handling
                break;
        }
    }

    return resolved;
}

ResolvedTableConstraint SemanticAnalyzerV2::analyzeTableConstraint(
    TableConstraint* constraint,
    const std::vector<ResolvedColumnDef>& columns)
{
    ResolvedTableConstraint resolved;

    if (!constraint) {
        return resolved;
    }

    resolved.name = constraint->name;

    switch (constraint->type) {
        case TableConstraintType::PRIMARY_KEY:
            resolved.constraint_type = ResolvedTableConstraint::Type::PRIMARY_KEY;
            // Resolve column names to indexes
            for (auto col_name : constraint->columns) {
                for (uint32_t i = 0; i < columns.size(); ++i) {
                    if (columns[i].name == col_name) {
                        resolved.column_indexes.push_back(i);
                        break;
                    }
                }
            }
            break;

        case TableConstraintType::UNIQUE:
            resolved.constraint_type = ResolvedTableConstraint::Type::UNIQUE;
            for (auto col_name : constraint->columns) {
                for (uint32_t i = 0; i < columns.size(); ++i) {
                    if (columns[i].name == col_name) {
                        resolved.column_indexes.push_back(i);
                        break;
                    }
                }
            }
            break;

        case TableConstraintType::FOREIGN_KEY:
            resolved.constraint_type = ResolvedTableConstraint::Type::FOREIGN_KEY;
            // Resolve local columns
            for (auto col_name : constraint->columns) {
                for (uint32_t i = 0; i < columns.size(); ++i) {
                    if (columns[i].name == col_name) {
                        resolved.column_indexes.push_back(i);
                        break;
                    }
                }
            }
            // Resolve referenced table and columns
            if (!constraint->ref_table.components.empty()) {
                resolved.fk_table_path = constraint->ref_table;
                auto ref_table = resolveTable(constraint->ref_table, SourceSpan{}, false);
                if (ref_table) {
                    resolved.fk_table_uuid = ref_table->table_uuid;
                    for (auto ref_col_name : constraint->ref_columns) {
                        for (uint32_t i = 0; i < ref_table->columns.size(); ++i) {
                            if (ref_table->columns[i].name == ref_col_name) {
                                resolved.fk_column_indexes.push_back(i);
                                break;
                            }
                        }
                        if (ref_col_name != StringPool::INVALID_ID) {
                            resolved.fk_column_names.push_back(ref_col_name);
                        }
                    }
                    if (resolved.fk_column_names.empty()) {
                        for (auto col_name : constraint->columns) {
                            resolved.fk_column_names.push_back(col_name);
                        }
                    }
                }
            }
            resolved.on_delete = constraint->on_delete;
            resolved.on_update = constraint->on_update;
            break;

        case TableConstraintType::CHECK:
            resolved.constraint_type = ResolvedTableConstraint::Type::CHECK;
            if (constraint->check_expr) {
                resolved.check_expr = analyzeExpression(constraint->check_expr);
            }
            break;

        case TableConstraintType::EXCLUDE:
            // Exclusion constraints are PostgreSQL-specific
            // Would need additional handling
            break;
    }

    return resolved;
}

// =============================================================================
// Utility Methods
// =============================================================================

std::string_view SemanticAnalyzerV2::getString(StringPool::StringId id) const {
    return string_pool_.get(id);
}

StringPool::StringId SemanticAnalyzerV2::internString(std::string_view str) {
    return string_pool_.intern(str);
}

bool SemanticAnalyzerV2::isAggregate(const ResolvedExpression* expr) const {
    if (!expr) {
        return false;
    }

    if (auto* func = dynamic_cast<const ResolvedFunctionCall*>(expr)) {
        return func->function.is_aggregate;
    }

    return false;
}

bool SemanticAnalyzerV2::validateGroupBy(ResolvedSelectStmt* stmt) {
    if (!stmt) {
        return false;
    }

    std::unordered_set<GroupByColumnKey, GroupByColumnKeyHash> grouped_columns;
    for (auto* expr : stmt->group_by) {
        if (containsAggregateExpr(expr)) {
            error(expr->span, "GROUP BY clause cannot contain aggregate functions");
            return false;
        }
        if (auto* col = dynamic_cast<const ResolvedColumnRefExpr*>(expr)) {
            grouped_columns.insert({col->column.table_uuid, col->column.column_index});
        }
    }

    auto isGroupedExpression = [&](const ResolvedExpression* expr) -> bool {
        for (auto* grouped : stmt->group_by) {
            if (expressionsEqual(expr, grouped)) {
                return true;
            }
        }
        return false;
    };

    for (const auto& item : stmt->select_list) {
        if (!item.expr) {
            continue;
        }

        if (containsAggregateExpr(item.expr)) {
            continue;
        }

        if (isConstantExpr(item.expr)) {
            continue;
        }

        if (isGroupedExpression(item.expr)) {
            continue;
        }

        if (auto* ungrouped = findUngroupedColumn(item.expr, grouped_columns)) {
            std::string col_name = std::string(getString(ungrouped->column.column_name));
            error(item.expr->span,
                  "Column '" + col_name + "' must appear in the GROUP BY clause or be used in an aggregate function");
            return false;
        }

        if (containsColumnRef(item.expr)) {
            continue;  // All column refs are grouped.
        }

        error(item.expr->span,
              "SELECT expression must appear in the GROUP BY clause or be used in an aggregate function");
        return false;
    }

    return true;
}

} // namespace scratchbird::parser::v2
