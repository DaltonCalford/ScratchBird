#include "scratchbird/sblr/extract_element_catalog.h"
#include <algorithm>
#include <string>
#include <unordered_map>

namespace scratchbird::sblr
{
    namespace
    {
        std::string toUpperAscii(std::string_view input)
        {
            std::string result;
            result.reserve(input.size());
            for (char c : input)
            {
                result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            }
            return result;
        }

        const std::unordered_map<std::string, ExtractField> kFieldMap = {
            // Temporal
            {"YEAR", ExtractField::YEAR},
            {"YEARS", ExtractField::YEAR},
            {"MONTH", ExtractField::MONTH},
            {"MONTHS", ExtractField::MONTH},
            {"DAY", ExtractField::DAY},
            {"DAYS", ExtractField::DAY},
            {"DAYOFMONTH", ExtractField::DAY},
            {"DAY_OF_MONTH", ExtractField::DAY},
            {"HOUR", ExtractField::HOUR},
            {"HOURS", ExtractField::HOUR},
            {"HOUR24", ExtractField::HOUR},
            {"HOUR_24", ExtractField::HOUR},
            {"HOUR12", ExtractField::HOUR12},
            {"HOUR_12", ExtractField::HOUR12},
            {"MINUTE", ExtractField::MINUTE},
            {"MINUTES", ExtractField::MINUTE},
            {"SECOND", ExtractField::SECOND},
            {"SECONDS", ExtractField::SECOND},
            {"MICROSECOND", ExtractField::MICROSECOND},
            {"MICROSECONDS", ExtractField::MICROSECOND},
            {"MILLISECOND", ExtractField::MILLISECOND},
            {"MILLISECONDS", ExtractField::MILLISECOND},
            {"DOW", ExtractField::DOW},
            {"DAYOFWEEK", ExtractField::DOW},
            {"DAY_OF_WEEK", ExtractField::DOW},
            {"WEEKDAY", ExtractField::DOW},
            {"ISODOW", ExtractField::ISO_DOW},
            {"ISO_DOW", ExtractField::ISO_DOW},
            {"DOY", ExtractField::DOY},
            {"DAYOFYEAR", ExtractField::DOY},
            {"DAY_OF_YEAR", ExtractField::DOY},
            {"QUARTER", ExtractField::QUARTER},
            {"QUARTERS", ExtractField::QUARTER},
            {"WEEK", ExtractField::WEEK},
            {"WEEKS", ExtractField::WEEK},
            {"ISO_WEEK", ExtractField::ISO_WEEK},
            {"ISOWEEK", ExtractField::ISO_WEEK},
            {"ISO_YEAR", ExtractField::ISO_YEAR},
            {"ISOYEAR", ExtractField::ISO_YEAR},
            {"CENTURY", ExtractField::CENTURY},
            {"CENTURIES", ExtractField::CENTURY},
            {"DECADE", ExtractField::DECADE},
            {"DECADES", ExtractField::DECADE},
            {"MILLENNIUM", ExtractField::MILLENNIUM},
            {"MILLENNIA", ExtractField::MILLENNIUM},
            {"EPOCH", ExtractField::EPOCH},
            {"TIMEZONE", ExtractField::TIMEZONE},
            {"TZ", ExtractField::TIMEZONE},
            {"TIMEZONE_HOUR", ExtractField::TIMEZONE_HOUR},
            {"TIMEZONEHOUR", ExtractField::TIMEZONE_HOUR},
            {"TZHOUR", ExtractField::TIMEZONE_HOUR},
            {"TZ_HOUR", ExtractField::TIMEZONE_HOUR},
            {"TIMEZONE_MINUTE", ExtractField::TIMEZONE_MINUTE},
            {"TIMEZONEMINUTE", ExtractField::TIMEZONE_MINUTE},
            {"TZMINUTE", ExtractField::TIMEZONE_MINUTE},
            {"TZ_MINUTE", ExtractField::TIMEZONE_MINUTE},
            {"TZ_OFFSET", ExtractField::TZ_OFFSET},
            {"TIMEZONE_OFFSET", ExtractField::TZ_OFFSET},
            {"TZOFFSET", ExtractField::TZ_OFFSET},
            {"TOTAL_MONTHS", ExtractField::TOTAL_MONTHS},
            {"TOTALMONTHS", ExtractField::TOTAL_MONTHS},
            {"TOTAL_DAYS", ExtractField::TOTAL_DAYS},
            {"TOTALDAYS", ExtractField::TOTAL_DAYS},
            {"TOTAL_SECONDS", ExtractField::TOTAL_SECONDS},
            {"TOTALSECONDS", ExtractField::TOTAL_SECONDS},

            // UUID
            {"VERSION", ExtractField::VERSION},
            {"VARIANT", ExtractField::VARIANT},
            {"TIMESTAMP", ExtractField::TIMESTAMP},
            {"NODE", ExtractField::NODE},
            {"CLOCK_SEQ", ExtractField::CLOCK_SEQ},
            {"CLOCKSEQ", ExtractField::CLOCK_SEQ},
            {"TIME_LOW", ExtractField::TIME_LOW},
            {"TIME_LOWEST", ExtractField::TIME_LOW},
            {"TIME_MID", ExtractField::TIME_MID},
            {"TIME_HIGH", ExtractField::TIME_HIGH},
            {"RAND_A", ExtractField::RAND_A},
            {"RAND_B", ExtractField::RAND_B},

            // Network
            {"FAMILY", ExtractField::FAMILY},
            {"NETMASK", ExtractField::NETMASK},
            {"ADDRESS", ExtractField::ADDRESS},
            {"NETWORK", ExtractField::NETWORK},
            {"BROADCAST", ExtractField::BROADCAST},
            {"HOSTMASK", ExtractField::HOSTMASK},
            {"NETMASK_ADDR", ExtractField::NETMASK_ADDR},
            {"NETMASKADDR", ExtractField::NETMASK_ADDR},
            {"IS_IPV4", ExtractField::IS_IPV4},
            {"IS_IPV6", ExtractField::IS_IPV6},
            {"OUI", ExtractField::OUI},
            {"VENDOR", ExtractField::VENDOR},
            {"NIC", ExtractField::NIC},
            {"IS_MULTICAST", ExtractField::IS_MULTICAST},
            {"IS_LOCAL", ExtractField::IS_LOCAL},
            {"TRUNC", ExtractField::TRUNC},

            // Spatial
            {"X", ExtractField::X},
            {"Y", ExtractField::Y},
            {"SRID", ExtractField::SRID},
            {"NUM_POINTS", ExtractField::NUM_POINTS},
            {"START_POINT", ExtractField::START_POINT},
            {"END_POINT", ExtractField::END_POINT},
            {"NUM_RINGS", ExtractField::NUM_RINGS},
            {"EXTERIOR_RING", ExtractField::EXTERIOR_RING},
            {"NUM_INTERIOR_RINGS", ExtractField::NUM_INTERIOR_RINGS},
            {"NUM_GEOMETRIES", ExtractField::NUM_GEOMETRIES},
            {"BBOX", ExtractField::BBOX},
            {"POINTS", ExtractField::POINTS},
            {"RINGS", ExtractField::RINGS},
            {"AREA", ExtractField::AREA},
            {"GEOMETRIES", ExtractField::GEOMETRIES},
            {"LENGTH", ExtractField::LENGTH},

            // Array / Composite / Variant
            {"CARDINALITY", ExtractField::CARDINALITY},
            {"NDIMS", ExtractField::NDIMS},
            {"DIMS", ExtractField::DIMS},
            {"LOWER", ExtractField::LOWER},
            {"UPPER", ExtractField::UPPER},
            {"LOWER_VALUE", ExtractField::LOWER_VALUE},
            {"UPPER_VALUE", ExtractField::UPPER_VALUE},
            {"LOWER_INC", ExtractField::LOWER_INC},
            {"UPPER_INC", ExtractField::UPPER_INC},
            {"LOWER_INF", ExtractField::LOWER_INF},
            {"UPPER_INF", ExtractField::UPPER_INF},
            {"IS_EMPTY", ExtractField::ISEMPTY},
            {"ISEMPTY", ExtractField::ISEMPTY},
            {"IS_LOWER_INFINITE", ExtractField::LOWER_INF},
            {"IS_UPPER_INFINITE", ExtractField::UPPER_INF},
            {"ELEMENT", ExtractField::ELEMENT},
            {"FIELD", ExtractField::FIELD},
            {"FIELD_NAMES", ExtractField::FIELD_NAMES},
            {"DATATYPE", ExtractField::DATATYPE},

            // Numeric / generic
            {"VALUE", ExtractField::VALUE},
            {"SIGN", ExtractField::SIGN},
            {"ABS", ExtractField::ABS},
            {"BYTES", ExtractField::BYTES},
            {"BITS", ExtractField::BITS},
            {"HI64", ExtractField::HI64},
            {"LO64", ExtractField::LO64},
            {"EXPONENT", ExtractField::EXPONENT},
            {"MANTISSA", ExtractField::MANTISSA},
            {"IS_NAN", ExtractField::IS_NAN},
            {"ISNAN", ExtractField::IS_NAN},
            {"IS_INF", ExtractField::IS_INF},
            {"ISINF", ExtractField::IS_INF},
            {"PRECISION", ExtractField::PRECISION},
            {"SCALE", ExtractField::SCALE},
            {"UNSCALED", ExtractField::UNSCALED},
            {"MAJOR", ExtractField::MAJOR},
            {"MINOR", ExtractField::MINOR},

            // String / document
            {"CHAR_LENGTH", ExtractField::CHAR_LENGTH},
            {"CHARACTER_LENGTH", ExtractField::CHAR_LENGTH},
            {"OCTET_LENGTH", ExtractField::OCTET_LENGTH},
            {"BYTE_LENGTH", ExtractField::OCTET_LENGTH},
            {"CODEPOINT_LENGTH", ExtractField::CODEPOINT_LENGTH},
            {"TRIMMED_LENGTH", ExtractField::TRIMMED_LENGTH},
            {"TYPE", ExtractField::TYPE},
            {"KEYS", ExtractField::KEYS},
            {"PATH", ExtractField::PATH},
            {"ATTRIBUTES", ExtractField::ATTRIBUTES},

            // Binary / vector
            {"BYTE", ExtractField::BYTE},
            {"BIT", ExtractField::BIT},
            {"SLICE", ExtractField::SLICE},
            {"DIGEST", ExtractField::DIGEST},
            {"DIMENSION", ExtractField::DIMENSION},
            {"NORM_L2", ExtractField::NORM_L2},
            {"NORM", ExtractField::NORM_L2},
            {"DOT", ExtractField::DOT},

            // Text search
            {"LEXEMES", ExtractField::LEXEMES},
            {"POSITIONS", ExtractField::POSITIONS},
            {"WEIGHTS", ExtractField::WEIGHTS},
            {"SIZE", ExtractField::SIZE},
            {"HAS_LEXEME", ExtractField::HAS_LEXEME},
            {"ROOT_OP", ExtractField::ROOT_OP},
            {"TERMS", ExtractField::TERMS},
            {"OPERATORS", ExtractField::OPERATORS},
            {"PHRASE_DISTANCE", ExtractField::PHRASE_DISTANCE},
            {"NODES", ExtractField::NODES},
        };
    }

    std::optional<ExtractField> resolveExtractFieldName(std::string_view name)
    {
        if (name.empty())
        {
            return std::nullopt;
        }
        std::string upper = toUpperAscii(name);
        auto it = kFieldMap.find(upper);
        if (it == kFieldMap.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    const char* extractFieldToString(ExtractField field)
    {
        switch (field)
        {
            case ExtractField::YEAR: return "YEAR";
            case ExtractField::MONTH: return "MONTH";
            case ExtractField::DAY: return "DAY";
            case ExtractField::HOUR: return "HOUR";
            case ExtractField::MINUTE: return "MINUTE";
            case ExtractField::SECOND: return "SECOND";
            case ExtractField::MICROSECOND: return "MICROSECOND";
            case ExtractField::MILLISECOND: return "MILLISECOND";
            case ExtractField::DOW: return "DOW";
            case ExtractField::DOY: return "DOY";
            case ExtractField::QUARTER: return "QUARTER";
            case ExtractField::WEEK: return "WEEK";
            case ExtractField::EPOCH: return "EPOCH";
            case ExtractField::TIMEZONE: return "TIMEZONE";
            case ExtractField::TIMEZONE_HOUR: return "TIMEZONE_HOUR";
            case ExtractField::TIMEZONE_MINUTE: return "TIMEZONE_MINUTE";
            case ExtractField::TZ_OFFSET: return "TZ_OFFSET";
            case ExtractField::ISO_WEEK: return "ISO_WEEK";
            case ExtractField::ISO_YEAR: return "ISO_YEAR";
            case ExtractField::ISO_DOW: return "ISO_DOW";
            case ExtractField::CENTURY: return "CENTURY";
            case ExtractField::DECADE: return "DECADE";
            case ExtractField::MILLENNIUM: return "MILLENNIUM";
            case ExtractField::HOUR12: return "HOUR12";
            case ExtractField::TOTAL_MONTHS: return "TOTAL_MONTHS";
            case ExtractField::TOTAL_DAYS: return "TOTAL_DAYS";
            case ExtractField::TOTAL_SECONDS: return "TOTAL_SECONDS";
            case ExtractField::VERSION: return "VERSION";
            case ExtractField::VARIANT: return "VARIANT";
            case ExtractField::TIMESTAMP: return "TIMESTAMP";
            case ExtractField::NODE: return "NODE";
            case ExtractField::CLOCK_SEQ: return "CLOCK_SEQ";
            case ExtractField::TIME_LOW: return "TIME_LOW";
            case ExtractField::TIME_MID: return "TIME_MID";
            case ExtractField::TIME_HIGH: return "TIME_HIGH";
            case ExtractField::RAND_A: return "RAND_A";
            case ExtractField::RAND_B: return "RAND_B";
            case ExtractField::FAMILY: return "FAMILY";
            case ExtractField::NETMASK: return "NETMASK";
            case ExtractField::ADDRESS: return "ADDRESS";
            case ExtractField::NETWORK: return "NETWORK";
            case ExtractField::BROADCAST: return "BROADCAST";
            case ExtractField::HOSTMASK: return "HOSTMASK";
            case ExtractField::VENDOR: return "VENDOR";
            case ExtractField::NETMASK_ADDR: return "NETMASK_ADDR";
            case ExtractField::IS_IPV4: return "IS_IPV4";
            case ExtractField::IS_IPV6: return "IS_IPV6";
            case ExtractField::OUI: return "OUI";
            case ExtractField::NIC: return "NIC";
            case ExtractField::IS_MULTICAST: return "IS_MULTICAST";
            case ExtractField::IS_LOCAL: return "IS_LOCAL";
            case ExtractField::TRUNC: return "TRUNC";
            case ExtractField::X: return "X";
            case ExtractField::Y: return "Y";
            case ExtractField::SRID: return "SRID";
            case ExtractField::NUM_POINTS: return "NUM_POINTS";
            case ExtractField::START_POINT: return "START_POINT";
            case ExtractField::END_POINT: return "END_POINT";
            case ExtractField::NUM_RINGS: return "NUM_RINGS";
            case ExtractField::EXTERIOR_RING: return "EXTERIOR_RING";
            case ExtractField::NUM_INTERIOR_RINGS: return "NUM_INTERIOR_RINGS";
            case ExtractField::NUM_GEOMETRIES: return "NUM_GEOMETRIES";
            case ExtractField::BBOX: return "BBOX";
            case ExtractField::POINTS: return "POINTS";
            case ExtractField::RINGS: return "RINGS";
            case ExtractField::AREA: return "AREA";
            case ExtractField::GEOMETRIES: return "GEOMETRIES";
            case ExtractField::LENGTH: return "LENGTH";
            case ExtractField::CARDINALITY: return "CARDINALITY";
            case ExtractField::NDIMS: return "NDIMS";
            case ExtractField::DIMS: return "DIMS";
            case ExtractField::LOWER: return "LOWER";
            case ExtractField::UPPER: return "UPPER";
            case ExtractField::ELEMENT: return "ELEMENT";
            case ExtractField::LOWER_VALUE: return "LOWER_VALUE";
            case ExtractField::UPPER_VALUE: return "UPPER_VALUE";
            case ExtractField::LOWER_INC: return "LOWER_INC";
            case ExtractField::UPPER_INC: return "UPPER_INC";
            case ExtractField::LOWER_INF: return "LOWER_INF";
            case ExtractField::UPPER_INF: return "UPPER_INF";
            case ExtractField::ISEMPTY: return "IS_EMPTY";
            case ExtractField::VALUE: return "VALUE";
            case ExtractField::SIGN: return "SIGN";
            case ExtractField::ABS: return "ABS";
            case ExtractField::BYTES: return "BYTES";
            case ExtractField::BITS: return "BITS";
            case ExtractField::HI64: return "HI64";
            case ExtractField::LO64: return "LO64";
            case ExtractField::EXPONENT: return "EXPONENT";
            case ExtractField::MANTISSA: return "MANTISSA";
            case ExtractField::IS_NAN: return "IS_NAN";
            case ExtractField::IS_INF: return "IS_INF";
            case ExtractField::PRECISION: return "PRECISION";
            case ExtractField::SCALE: return "SCALE";
            case ExtractField::UNSCALED: return "UNSCALED";
            case ExtractField::MAJOR: return "MAJOR";
            case ExtractField::MINOR: return "MINOR";
            case ExtractField::CHAR_LENGTH: return "CHAR_LENGTH";
            case ExtractField::OCTET_LENGTH: return "OCTET_LENGTH";
            case ExtractField::CODEPOINT_LENGTH: return "CODEPOINT_LENGTH";
            case ExtractField::TRIMMED_LENGTH: return "TRIMMED_LENGTH";
            case ExtractField::TYPE: return "TYPE";
            case ExtractField::KEYS: return "KEYS";
            case ExtractField::PATH: return "PATH";
            case ExtractField::ATTRIBUTES: return "ATTRIBUTES";
            case ExtractField::BYTE: return "BYTE";
            case ExtractField::BIT: return "BIT";
            case ExtractField::SLICE: return "SLICE";
            case ExtractField::DIGEST: return "DIGEST";
            case ExtractField::DIMENSION: return "DIMENSION";
            case ExtractField::NORM_L2: return "NORM_L2";
            case ExtractField::DOT: return "DOT";
            case ExtractField::FIELD: return "FIELD";
            case ExtractField::FIELD_NAMES: return "FIELD_NAMES";
            case ExtractField::DATATYPE: return "DATATYPE";
            case ExtractField::LEXEMES: return "LEXEMES";
            case ExtractField::POSITIONS: return "POSITIONS";
            case ExtractField::WEIGHTS: return "WEIGHTS";
            case ExtractField::SIZE: return "SIZE";
            case ExtractField::HAS_LEXEME: return "HAS_LEXEME";
            case ExtractField::ROOT_OP: return "ROOT_OP";
            case ExtractField::TERMS: return "TERMS";
            case ExtractField::OPERATORS: return "OPERATORS";
            case ExtractField::PHRASE_DISTANCE: return "PHRASE_DISTANCE";
            case ExtractField::NODES: return "NODES";
            default: return "UNKNOWN";
        }
    }

    ElementArgSpec extractFieldArgSpec(ExtractField field)
    {
        switch (field)
        {
            case ExtractField::ELEMENT:
            case ExtractField::BYTE:
            case ExtractField::BIT:
            case ExtractField::FIELD:
            case ExtractField::PATH:
            case ExtractField::HAS_LEXEME:
            case ExtractField::DOT:
                return {1, 1};
            case ExtractField::SLICE:
                return {2, 2};
            case ExtractField::DIGEST:
                return {1, 1};
            case ExtractField::LOWER:
            case ExtractField::UPPER:
            case ExtractField::LOWER_VALUE:
            case ExtractField::UPPER_VALUE:
                return {0, 1};
            default:
                return {0, 0};
        }
    }
} // namespace scratchbird::sblr
