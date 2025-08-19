#ifndef SCRATCHBIRD_ENGINE_TYPES_H
#define SCRATCHBIRD_ENGINE_TYPES_H

#include <cstdint>
#include <string>
#include <vector>

namespace scratchbird
{
    namespace engine
    {

        enum class TypeKind {
            Unknown,
            // Numeric
            TinyInt,
            SmallInt,
            Integer,
            BigInt,
            Int128,
            UTinyInt,
            USmallInt,
            UInteger,
            UBigInt,
            UInt128,
            Numeric,
            Decimal,
            Float,
            DoublePrecision,
            DecFloat,
            // Text
            Char,
            VarChar,
            CiText,
            // Binary/Other
            Blob,
            Boolean,
            Uuid,
            Json,
            // Temporal
            Date,
            Time,
            TimeTz,
            Timestamp,
            TimestampTz,
            // Network
            Inet,
            Cidr,
            MacAddr,
            // Ranges
            Int4Range,
            Int8Range,
            NumRange,
            DateRange,
            TsRange,
            TstzRange,
            // Geometry/FTS/ML
            Point,
            TsVector,
            TsQuery,
            Vector,
            // Composite and Arrays
            Composite,
            Array
        };

        struct TypeDescriptor {
            TypeKind kind{TypeKind::Unknown};
            int32_t length{-1};      // for CHAR/VARCHAR
            int32_t precision{-1};   // for NUMERIC/DECIMAL/FLOAT/DECFLOAT
            int32_t scale{-1};       // for NUMERIC/DECIMAL
            int32_t vector_dims{-1}; // for VECTOR(n)
            std::string collation;   // for text
            std::string charset;     // for text
        };

        TypeDescriptor parse_type_spec(const std::string& sql_type);
        std::string to_string(const TypeDescriptor& td);

    } // namespace engine
} // namespace scratchbird

#endif // SCRATCHBIRD_ENGINE_TYPES_H
