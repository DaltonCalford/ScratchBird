#include "scratchbird/core/typed_value.h"
#include "scratchbird/core/data_encryption.h"
#include "scratchbird/core/decimal.h"
#include "scratchbird/core/firebird_datetime.h"
#include "scratchbird/core/network.h"
#include "scratchbird/core/decfloat.h"
#include "scratchbird/core/tsvector.h"
#include "scratchbird/core/tsquery.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/timezone.h"
#include "scratchbird/core/utf8_utils.h"
#include "scratchbird/spatial/wkt_parser.h"
#include <nlohmann/json.hpp>
#include <cstring>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <charconv>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <algorithm>

namespace scratchbird::core
{
    namespace
    {
        void appendUint8(std::vector<uint8_t> &out, uint8_t value)
        {
            out.push_back(value);
        }

        void appendUint16(std::vector<uint8_t> &out, uint16_t value)
        {
            out.push_back(static_cast<uint8_t>(value & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
        }

        void appendUint32(std::vector<uint8_t> &out, uint32_t value)
        {
            out.push_back(static_cast<uint8_t>(value & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
            out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
        }

        void appendUint64(std::vector<uint8_t> &out, uint64_t value)
        {
            for (int i = 0; i < 8; ++i)
            {
                out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
            }
        }

        void appendInt32(std::vector<uint8_t> &out, int32_t value)
        {
            appendUint32(out, static_cast<uint32_t>(value));
        }

        void appendInt64(std::vector<uint8_t> &out, int64_t value)
        {
            appendUint64(out, static_cast<uint64_t>(value));
        }

        void appendFloat(std::vector<uint8_t> &out, float value)
        {
            uint32_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            appendUint32(out, bits);
        }

        void appendDouble(std::vector<uint8_t> &out, double value)
        {
            uint64_t bits = 0;
            std::memcpy(&bits, &value, sizeof(bits));
            appendUint64(out, bits);
        }

        void appendInt128(std::vector<uint8_t> &out, int128_t value, size_t bytes)
        {
            uint128_t uvalue = static_cast<uint128_t>(value);
            for (size_t i = 0; i < bytes; ++i)
            {
                out.push_back(static_cast<uint8_t>((uvalue >> (i * 8)) & 0xFF));
            }
        }

        void appendUint128(std::vector<uint8_t> &out, uint128_t value, size_t bytes)
        {
            for (size_t i = 0; i < bytes; ++i)
            {
                out.push_back(static_cast<uint8_t>((value >> (i * 8)) & 0xFF));
            }
        }

        bool readUint8(const std::vector<uint8_t> &data, size_t &offset, uint8_t &value)
        {
            if (offset + 1 > data.size())
            {
                return false;
            }
            value = data[offset];
            offset += 1;
            return true;
        }

        bool readUint16(const std::vector<uint8_t> &data, size_t &offset, uint16_t &value)
        {
            if (offset + 2 > data.size())
            {
                return false;
            }
            value = static_cast<uint16_t>(data[offset]) |
                    static_cast<uint16_t>(data[offset + 1] << 8);
            offset += 2;
            return true;
        }

        bool readUint32(const std::vector<uint8_t> &data, size_t &offset, uint32_t &value)
        {
            if (offset + 4 > data.size())
            {
                return false;
            }
            value = static_cast<uint32_t>(data[offset]) |
                    (static_cast<uint32_t>(data[offset + 1]) << 8) |
                    (static_cast<uint32_t>(data[offset + 2]) << 16) |
                    (static_cast<uint32_t>(data[offset + 3]) << 24);
            offset += 4;
            return true;
        }

        bool readUint64(const std::vector<uint8_t> &data, size_t &offset, uint64_t &value)
        {
            if (offset + 8 > data.size())
            {
                return false;
            }
            value = 0;
            for (int i = 0; i < 8; ++i)
            {
                value |= (static_cast<uint64_t>(data[offset + i]) << (i * 8));
            }
            offset += 8;
            return true;
        }


        bool readInt32(const std::vector<uint8_t> &data, size_t &offset, int32_t &value)
        {
            uint32_t tmp = 0;
            if (!readUint32(data, offset, tmp))
            {
                return false;
            }
            value = static_cast<int32_t>(tmp);
            return true;
        }

        bool readInt64(const std::vector<uint8_t> &data, size_t &offset, int64_t &value)
        {
            uint64_t tmp = 0;
            if (!readUint64(data, offset, tmp))
            {
                return false;
            }
            value = static_cast<int64_t>(tmp);
            return true;
        }

        bool readFloat(const std::vector<uint8_t> &data, size_t &offset, float &value)
        {
            uint32_t bits = 0;
            if (!readUint32(data, offset, bits))
            {
                return false;
            }
            std::memcpy(&value, &bits, sizeof(value));
            return true;
        }

        bool readDouble(const std::vector<uint8_t> &data, size_t &offset, double &value)
        {
            uint64_t bits = 0;
            if (!readUint64(data, offset, bits))
            {
                return false;
            }
            std::memcpy(&value, &bits, sizeof(value));
            return true;
        }

        bool readInt128(const std::vector<uint8_t> &data, size_t &offset, size_t bytes,
                        int128_t &value)
        {
            if (bytes == 0 || bytes > 16 || offset + bytes > data.size())
            {
                return false;
            }

            uint128_t uvalue = 0;
            for (size_t i = 0; i < bytes; ++i)
            {
                uvalue |= (static_cast<uint128_t>(data[offset + i]) << (i * 8));
            }

            if (bytes < 16 && (data[offset + bytes - 1] & 0x80))
            {
                uint128_t sign_mask = ~((static_cast<uint128_t>(1) << (bytes * 8)) - 1);
                uvalue |= sign_mask;
            }

            value = static_cast<int128_t>(uvalue);
            offset += bytes;
            return true;
        }

        bool readUint128(const std::vector<uint8_t> &data, size_t &offset, size_t bytes,
                         uint128_t &value)
        {
            if (bytes == 0 || bytes > 16 || offset + bytes > data.size())
            {
                return false;
            }

            uint128_t uvalue = 0;
            for (size_t i = 0; i < bytes; ++i)
            {
                uvalue |= (static_cast<uint128_t>(data[offset + i]) << (i * 8));
            }

            value = uvalue;
            offset += bytes;
            return true;
        }

        size_t decimalStorageSize(uint8_t precision)
        {
            if (precision <= 2)
            {
                return 1;
            }
            if (precision <= 4)
            {
                return 2;
            }
            if (precision <= 9)
            {
                return 4;
            }
            if (precision <= 18)
            {
                return 8;
            }
            if (precision <= 38)
            {
                return 16;
            }
            return 0;
        }

        std::string trimAscii(const std::string &input)
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

        std::string formatOffsetSeconds(int32_t offset_seconds)
        {
            int32_t total_minutes = offset_seconds / 60;
            int32_t hours = total_minutes / 60;
            int32_t minutes = std::abs(total_minutes % 60);
            std::ostringstream oss;
            oss << (offset_seconds >= 0 ? '+' : '-') << std::setfill('0') << std::setw(2)
                << std::abs(hours) << ':' << std::setfill('0') << std::setw(2) << minutes;
            return oss.str();
        }

        int64_t floorDiv(int64_t value, int64_t divisor)
        {
            int64_t quotient = value / divisor;
            int64_t remainder = value % divisor;
            if (remainder != 0 && ((remainder > 0) != (divisor > 0)))
            {
                --quotient;
            }
            return quotient;
        }

        using Json = nlohmann::json;
        using OrderedJson = nlohmann::ordered_json;

        OrderedJson canonicalizeJson(const Json& input)
        {
            if (input.is_object())
            {
                OrderedJson obj = OrderedJson::object();
                std::vector<std::string> keys;
                keys.reserve(input.size());
                for (auto it = input.begin(); it != input.end(); ++it)
                {
                    keys.push_back(it.key());
                }
                std::sort(keys.begin(), keys.end());
                for (const auto& key : keys)
                {
                    obj[key] = canonicalizeJson(input.at(key));
                }
                return obj;
            }
            if (input.is_array())
            {
                OrderedJson arr = OrderedJson::array();
                for (const auto& elem : input)
                {
                    arr.push_back(canonicalizeJson(elem));
                }
                return arr;
            }
            return input;
        }

        bool encodeJsonb(const std::string& text, std::vector<uint8_t>& out, ErrorContext* ctx)
        {
            try
            {
                Json parsed = Json::parse(text);
                OrderedJson canonical = canonicalizeJson(parsed);
                out = OrderedJson::to_cbor(canonical);
                return true;
            }
            catch (...)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION, "Invalid JSONB text");
                return false;
            }
        }

        bool decodeJsonb(const std::vector<uint8_t>& data, std::string& out)
        {
            try
            {
                Json parsed = Json::from_cbor(data);
                out = parsed.dump();
                return true;
            }
            catch (...)
            {
                return false;
            }
        }

        bool isStringLike(DataType type)
        {
            return type == DataType::CHAR || type == DataType::VARCHAR ||
                   type == DataType::TEXT || type == DataType::JSON ||
                   type == DataType::JSONB || type == DataType::XML;
        }

        bool isBinaryLike(DataType type)
        {
            return type == DataType::BINARY || type == DataType::VARBINARY ||
                   type == DataType::BLOB || type == DataType::BYTEA ||
                   type == DataType::VECTOR;
        }

        bool isIntegerType(DataType type)
        {
            return type == DataType::INT8 || type == DataType::INT16 ||
                   type == DataType::INT32 || type == DataType::INT64 ||
                   type == DataType::INT128 || type == DataType::UINT8 ||
                   type == DataType::UINT16 || type == DataType::UINT32 ||
                   type == DataType::UINT64 || type == DataType::UINT128;
        }

        bool isUnsignedType(DataType type)
        {
            return type == DataType::UINT8 || type == DataType::UINT16 ||
                   type == DataType::UINT32 || type == DataType::UINT64 ||
                   type == DataType::UINT128;
        }

        bool isFloatType(DataType type)
        {
            return type == DataType::FLOAT32 || type == DataType::FLOAT64;
        }

        DecFloatContext defaultDecfloatContext()
        {
            return DecFloatContext{};
        }

        Status decodeDecfloat(const std::vector<uint8_t>& bytes, DataType type,
                              DecFloat& out, ErrorContext* ctx)
        {
            if (type == DataType::DECFLOAT16)
            {
                if (bytes.size() != 8)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "DECFLOAT16 requires 8 bytes");
                    return Status::INVALID_ARGUMENT;
                }
                uint64_t bits = 0;
                size_t offset = 0;
                if (!readUint64(bytes, offset, bits))
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "DECFLOAT16 decode failed");
                    return Status::INVALID_ARGUMENT;
                }
                out = DecFloat::fromBID64(bits);
                return Status::OK;
            }
            if (type == DataType::DECFLOAT34)
            {
                if (bytes.size() != 16)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "DECFLOAT34 requires 16 bytes");
                    return Status::INVALID_ARGUMENT;
                }
                uint64_t low = 0;
                uint64_t high = 0;
                size_t offset = 0;
                if (!readUint64(bytes, offset, low) || !readUint64(bytes, offset, high))
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "DECFLOAT34 decode failed");
                    return Status::INVALID_ARGUMENT;
                }
                out = DecFloat::fromBID128(high, low);
                return Status::OK;
            }

            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Unsupported DECFLOAT type");
            return Status::INVALID_ARGUMENT;
        }

        Status encodeDecfloat(const DecFloat& value, DataType type,
                              std::vector<uint8_t>& bytes, ErrorContext* ctx)
        {
            bytes.clear();
            DecFloatContext df_ctx = defaultDecfloatContext();
            if (type == DataType::DECFLOAT16)
            {
                uint64_t bits = 0;
                Status st = value.toBID64(bits, df_ctx, ctx);
                if (st != Status::OK)
                {
                    return st;
                }
                appendUint64(bytes, bits);
                return Status::OK;
            }
            if (type == DataType::DECFLOAT34)
            {
                uint64_t high = 0;
                uint64_t low = 0;
                Status st = value.toBID128(high, low, df_ctx, ctx);
                if (st != Status::OK)
                {
                    return st;
                }
                appendUint64(bytes, low);
                appendUint64(bytes, high);
                return Status::OK;
            }

            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Unsupported DECFLOAT type");
            return Status::INVALID_ARGUMENT;
        }

        Status coerceToDecfloat(const TypedValue& source, DataType target,
                                DecFloat& out, ErrorContext* ctx)
        {
            DecFloatContext df_ctx = defaultDecfloatContext();
            if (source.type() == DataType::DECFLOAT16 || source.type() == DataType::DECFLOAT34)
            {
                return decodeDecfloat(source.getDecfloatBytes(), source.type(), out, ctx);
            }

            if (source.type() == DataType::DECIMAL)
            {
                uint8_t precision = source.getDecimalPrecision() == 0
                                        ? DECIMAL_MAX_PRECISION
                                        : source.getDecimalPrecision();
                Decimal dec(source.getDecimalUnscaled(), precision, source.getDecimalScale());
                return DecFloat::parse(dec.toStringWithPrecision(dec.scale()),
                                       target == DataType::DECFLOAT16 ? 16 : 34,
                                       df_ctx, out, ctx);
            }

            if (isIntegerType(source.type()) || source.type() == DataType::BOOLEAN)
            {
                std::string text = source.toString();
                return DecFloat::parse(text, target == DataType::DECFLOAT16 ? 16 : 34,
                                       df_ctx, out, ctx);
            }

            if (isFloatType(source.type()))
            {
                std::ostringstream oss;
                oss.setf(std::ios::scientific);
                oss << std::setprecision(std::numeric_limits<double>::max_digits10)
                    << (source.type() == DataType::FLOAT32
                            ? static_cast<double>(source.getFloat32())
                            : source.getFloat64());
                return DecFloat::parse(oss.str(), target == DataType::DECFLOAT16 ? 16 : 34,
                                       df_ctx, out, ctx);
            }

            if (source.type() == DataType::MONEY)
            {
                Decimal money(static_cast<int128_t>(source.getInt64()), 19, 4);
                return DecFloat::parse(money.toStringWithPrecision(4),
                                       target == DataType::DECFLOAT16 ? 16 : 34,
                                       df_ctx, out, ctx);
            }

            if (isStringLike(source.type()))
            {
                std::string text = trimAscii(source.toString());
                return DecFloat::parse(text, target == DataType::DECFLOAT16 ? 16 : 34,
                                       df_ctx, out, ctx);
            }

            SET_ERROR_CONTEXT(ctx, Status::DATATYPE_MISMATCH,
                              "Cannot convert to DECFLOAT");
            return Status::DATATYPE_MISMATCH;
        }

        Status decfloatToInt128(const DecFloat& value, int128_t& out, ErrorContext* ctx)
        {
            if (value.klass != DecFloatClass::Finite)
            {
                SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                  "DECFLOAT is not finite");
                return Status::NUMERIC_VALUE_OUT_OF_RANGE;
            }
            if (value.coefficient.empty())
            {
                out = 0;
                return Status::OK;
            }

            int32_t exp = value.exponent;
            if (exp < 0 && static_cast<size_t>(-exp) >= value.coefficient.size())
            {
                out = 0;
                return Status::OK;
            }

            size_t digits = value.coefficient.size();
            if (exp > 38)
            {
                SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                  "DECFLOAT value out of range");
                return Status::NUMERIC_VALUE_OUT_OF_RANGE;
            }
            if (exp > 0 && digits + static_cast<size_t>(exp) > 38)
            {
                SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                  "DECFLOAT value out of range");
                return Status::NUMERIC_VALUE_OUT_OF_RANGE;
            }

            int128_t coeff = 0;
            for (uint8_t digit : value.coefficient)
            {
                coeff = coeff * 10 + digit;
            }

            if (exp > 0)
            {
                coeff *= POWERS_OF_10[exp];
            }
            else if (exp < 0)
            {
                int32_t scale = -exp;
                if (scale > 38)
                {
                    out = 0;
                    return Status::OK;
                }
                coeff /= POWERS_OF_10[scale];
            }

            out = value.negative ? -coeff : coeff;
            return Status::OK;
        }

        Status decfloatToDecimal(const DecFloat& value, uint8_t precision, uint8_t scale,
                                 Decimal& out, ErrorContext* ctx)
        {
            if (value.klass != DecFloatClass::Finite)
            {
                SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                  "DECFLOAT is not finite");
                return Status::NUMERIC_VALUE_OUT_OF_RANGE;
            }

            int32_t exp = value.exponent;
            int32_t target_scale = scale;
            int128_t coeff = 0;
            for (uint8_t digit : value.coefficient)
            {
                coeff = coeff * 10 + digit;
            }
            if (exp >= 0)
            {
                if (exp > 38)
                {
                    SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                      "DECFLOAT value out of range");
                    return Status::NUMERIC_VALUE_OUT_OF_RANGE;
                }
                coeff *= POWERS_OF_10[exp];
            }
            else
            {
                int32_t scale_shift = -exp;
                if (scale_shift > 0)
                {
                    if (scale_shift > target_scale)
                    {
                        int32_t extra = scale_shift - target_scale;
                        if (extra >= static_cast<int32_t>(value.coefficient.size()))
                        {
                            coeff = 0;
                        }
                        else
                        {
                            coeff /= POWERS_OF_10[extra];
                        }
                    }
                }
            }

            out = Decimal(value.negative ? -coeff : coeff, precision, target_scale);
            return Status::OK;
        }

        bool isDecimalLike(DataType type)
        {
            return type == DataType::DECIMAL || type == DataType::MONEY ||
                   type == DataType::DECFLOAT16 || type == DataType::DECFLOAT34;
        }

        uint8_t defaultDecfloatPrecision(DataType type)
        {
            return type == DataType::DECFLOAT16 ? 16 : 34;
        }

        bool isNumericType(DataType type)
        {
            return isIntegerType(type) || isFloatType(type) ||
                   isDecimalLike(type) ||
                   type == DataType::BOOLEAN;
        }

        bool decodeHex(const std::string &text, std::vector<uint8_t> &out, ErrorContext *ctx);

        bool parseUuidString(const std::string &text, std::vector<uint8_t> &out,
                             ErrorContext *ctx)
        {
            std::string trimmed = trimAscii(text);
            if (trimmed.empty())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                  "Empty UUID string");
                return false;
            }

            if (trimmed.rfind("urn:uuid:", 0) == 0 || trimmed.rfind("URN:UUID:", 0) == 0)
            {
                trimmed = trimmed.substr(9);
            }

            std::string hex;
            hex.reserve(trimmed.size());
            for (char ch : trimmed)
            {
                if (ch == '{' || ch == '}' || ch == '-')
                {
                    continue;
                }
                hex.push_back(ch);
            }

            if (hex.size() != 32)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                  "Invalid UUID length");
                return false;
            }

            if (!decodeHex(hex, out, ctx))
            {
                return false;
            }

            if (out.size() != 16)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                  "Invalid UUID value");
                return false;
            }

            return true;
        }

        bool parseOffsetSuffix(const std::string &input, size_t min_pos,
                               std::string &base_out, int32_t &offset_seconds_out,
                               bool &has_offset_out, ErrorContext *ctx)
        {
            std::string trimmed = trimAscii(input);
            if (trimmed.empty())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_DATETIME_FORMAT, "Empty value");
                return false;
            }

            if (!trimmed.empty() && (trimmed.back() == 'Z' || trimmed.back() == 'z'))
            {
                base_out = trimAscii(trimmed.substr(0, trimmed.size() - 1));
                offset_seconds_out = 0;
                has_offset_out = true;
                return true;
            }

            size_t pos = trimmed.find_last_of("+-");
            if (pos != std::string::npos && pos >= min_pos)
            {
                std::string offset_str = trimmed.substr(pos);
                auto offset = TimezoneOffset::fromString(offset_str, ctx);
                if (!offset)
                {
                    return false;
                }
                base_out = trimAscii(trimmed.substr(0, pos));
                offset_seconds_out = static_cast<int32_t>(offset->offset_minutes) * 60;
                has_offset_out = true;
                return true;
            }

            base_out = trimmed;
            offset_seconds_out = 0;
            has_offset_out = false;
            return true;
        }

        bool parseIntegerString(const std::string &text, bool allow_signed,
                                int128_t &value_out, ErrorContext *ctx,
                                CastFormat format)
        {
            std::string trimmed = trimAscii(text);
            if (trimmed.empty())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                  "Empty string is not a number");
                return false;
            }

            const char *begin = trimmed.data();
            const char *end = trimmed.data() + trimmed.size();
            bool negative = false;
            if (*begin == '+' || *begin == '-')
            {
                if (*begin == '-')
                {
                    negative = true;
                }
                ++begin;
            }

            if (!allow_signed && negative)
            {
                SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                  "Negative value not allowed for unsigned type");
                return false;
            }

            int base = 10;
            if (format == CastFormat::HEX)
            {
                base = 16;
                if (end - begin >= 2 && begin[0] == '0' &&
                    (begin[1] == 'x' || begin[1] == 'X'))
                {
                    begin += 2;
                }
            }
            else if (end - begin >= 2 && begin[0] == '0' &&
                     (begin[1] == 'x' || begin[1] == 'X'))
            {
                base = 16;
                begin += 2;
            }

            if (begin == end)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                  "Invalid numeric format");
                return false;
            }

            uint128_t value = 0;
            uint128_t limit = allow_signed
                                  ? (negative ? (uint128_t{1} << 127)
                                              : ((uint128_t{1} << 127) - 1))
                                  : ~uint128_t{0};

            for (const char *ptr = begin; ptr < end; ++ptr)
            {
                int digit = -1;
                if (*ptr >= '0' && *ptr <= '9')
                {
                    digit = *ptr - '0';
                }
                else if (base == 16)
                {
                    if (*ptr >= 'a' && *ptr <= 'f')
                    {
                        digit = *ptr - 'a' + 10;
                    }
                    else if (*ptr >= 'A' && *ptr <= 'F')
                    {
                        digit = *ptr - 'A' + 10;
                    }
                }

                if (digit < 0 || digit >= base)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                      "Invalid numeric format");
                    return false;
                }
                if (value > (limit - static_cast<uint128_t>(digit)) /
                                static_cast<uint128_t>(base))
                {
                    SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                      "Numeric value out of range");
                    return false;
                }
                value = value * static_cast<uint128_t>(base) +
                        static_cast<uint128_t>(digit);
            }

            if (negative)
            {
                if (value == (uint128_t{1} << 127))
                {
                    value_out = std::numeric_limits<int128_t>::min();
                }
                else
                {
                    value_out = -static_cast<int128_t>(value);
                }
            }
            else
            {
                value_out = static_cast<int128_t>(value);
            }
            return true;
        }

        bool parseUnsignedIntegerString(const std::string &text, uint128_t &value_out,
                                        ErrorContext *ctx, CastFormat format)
        {
            std::string trimmed = trimAscii(text);
            if (trimmed.empty())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                  "Empty string is not a number");
                return false;
            }

            const char *begin = trimmed.data();
            const char *end = trimmed.data() + trimmed.size();
            if (*begin == '+' || *begin == '-')
            {
                if (*begin == '-')
                {
                    SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                      "Negative value not allowed for unsigned type");
                    return false;
                }
                ++begin;
            }

            int base = 10;
            if (format == CastFormat::HEX)
            {
                base = 16;
                if (end - begin >= 2 && begin[0] == '0' &&
                    (begin[1] == 'x' || begin[1] == 'X'))
                {
                    begin += 2;
                }
            }
            else if (end - begin >= 2 && begin[0] == '0' &&
                     (begin[1] == 'x' || begin[1] == 'X'))
            {
                base = 16;
                begin += 2;
            }

            if (begin == end)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                  "Invalid numeric format");
                return false;
            }

            uint128_t value = 0;
            uint128_t limit = ~uint128_t{0};

            for (const char *ptr = begin; ptr < end; ++ptr)
            {
                int digit = -1;
                if (*ptr >= '0' && *ptr <= '9')
                {
                    digit = *ptr - '0';
                }
                else if (base == 16)
                {
                    if (*ptr >= 'a' && *ptr <= 'f')
                    {
                        digit = *ptr - 'a' + 10;
                    }
                    else if (*ptr >= 'A' && *ptr <= 'F')
                    {
                        digit = *ptr - 'A' + 10;
                    }
                }

                if (digit < 0 || digit >= base)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                      "Invalid numeric format");
                    return false;
                }
                if (value > (limit - static_cast<uint128_t>(digit)) /
                                static_cast<uint128_t>(base))
                {
                    SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                      "Numeric value out of range");
                    return false;
                }
                value = value * static_cast<uint128_t>(base) +
                        static_cast<uint128_t>(digit);
            }

            value_out = value;
            return true;
        }

        bool parseFloatingString(const std::string &text, double &value_out,
                                 ErrorContext *ctx)
        {
            std::string trimmed = trimAscii(text);
            if (trimmed.empty())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                  "Empty string is not a number");
                return false;
            }

            char *end_ptr = nullptr;
            errno = 0;
            double val = std::strtod(trimmed.c_str(), &end_ptr);
            if (end_ptr == trimmed.c_str() || *end_ptr != '\0' || errno == ERANGE)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                  ("Invalid numeric format: " + trimmed).c_str());
                return false;
            }
            value_out = val;
            return true;
        }

        bool parseDateParts(const std::string &text, int &year, int &month, int &day)
        {
            return std::sscanf(text.c_str(), "%d-%d-%d", &year, &month, &day) == 3;
        }

        bool parseTimeParts(const std::string &text, int &hour, int &minute, int &second,
                            int &microseconds, ErrorContext *ctx)
        {
            hour = minute = second = microseconds = 0;
            if (text.empty())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_DATETIME_FORMAT, "Empty time value");
                return false;
            }

            std::string time_part = text;
            std::string frac_part;
            size_t dot_pos = text.find('.');
            if (dot_pos != std::string::npos)
            {
                time_part = text.substr(0, dot_pos);
                frac_part = text.substr(dot_pos + 1);
            }

            int parsed = std::sscanf(time_part.c_str(), "%d:%d:%d",
                                     &hour, &minute, &second);
            if (parsed < 2)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_DATETIME_FORMAT,
                                  "Invalid time format");
                return false;
            }
            if (parsed == 2)
            {
                second = 0;
            }

            if (!frac_part.empty())
            {
                if (frac_part.size() > 6)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_DATETIME_FORMAT,
                                      "Too many fractional second digits");
                    return false;
                }
                int frac_value = 0;
                for (char ch : frac_part)
                {
                    if (ch < '0' || ch > '9')
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_DATETIME_FORMAT,
                                          "Invalid fractional seconds");
                        return false;
                    }
                    frac_value = frac_value * 10 + (ch - '0');
                }
                int scale = 6 - static_cast<int>(frac_part.size());
                for (int i = 0; i < scale; ++i)
                {
                    frac_value *= 10;
                }
                microseconds = frac_value;
            }

            return true;
        }

        int64_t defaultDateTimeMicros()
        {
            Config &cfg = Config::getInstance();
            std::string default_time = cfg.getString("server.time", "date_default_time",
                                                     "00:00:00");
            int hour = 0;
            int minute = 0;
            int second = 0;
            int micros = 0;
            ErrorContext ctx;
            if (!parseTimeParts(default_time, hour, minute, second, micros, &ctx) ||
                hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
                second < 0 || second > 59)
            {
                return 0;
            }
            return (static_cast<int64_t>(hour) * 3600 +
                    static_cast<int64_t>(minute) * 60 +
                    static_cast<int64_t>(second)) * 1000000 +
                   micros;
        }

        std::string int128ToString(int128_t value)
        {
            if (value == 0)
            {
                return "0";
            }

            bool negative = value < 0;
            uint128_t uvalue = 0;
            if (negative)
            {
                if (value == std::numeric_limits<int128_t>::min())
                {
                    uvalue = uint128_t{1} << 127;
                }
                else
                {
                    uvalue = static_cast<uint128_t>(-value);
                }
            }
            else
            {
                uvalue = static_cast<uint128_t>(value);
            }

            std::string result;
            while (uvalue > 0)
            {
                uint8_t digit = static_cast<uint8_t>(uvalue % 10);
                result.push_back(static_cast<char>('0' + digit));
                uvalue /= 10;
            }
            if (negative)
            {
                result.push_back('-');
            }
            std::reverse(result.begin(), result.end());
            return result;
        }

        std::string uint128ToString(uint128_t value)
        {
            if (value == 0)
            {
                return "0";
            }

            std::string result;
            while (value > 0)
            {
                uint8_t digit = static_cast<uint8_t>(value % 10);
                result.push_back(static_cast<char>('0' + digit));
                value /= 10;
            }
            std::reverse(result.begin(), result.end());
            return result;
        }

        std::string uint128ToHex(uint128_t value)
        {
            if (value == 0)
            {
                return "0";
            }
            static const char kHexDigits[] = "0123456789abcdef";
            std::string result;
            while (value > 0)
            {
                uint8_t digit = static_cast<uint8_t>(value & 0x0F);
                result.push_back(kHexDigits[digit]);
                value >>= 4;
            }
            std::reverse(result.begin(), result.end());
            return result;
        }

        std::string formatSignedHex(int128_t value)
        {
            bool negative = value < 0;
            uint128_t magnitude = 0;
            if (negative)
            {
                if (value == std::numeric_limits<int128_t>::min())
                {
                    magnitude = uint128_t{1} << 127;
                }
                else
                {
                    magnitude = static_cast<uint128_t>(-value);
                }
            }
            else
            {
                magnitude = static_cast<uint128_t>(value);
            }

            std::string hex = uint128ToHex(magnitude);
            return (negative ? "-0x" : "0x") + hex;
        }

        std::string formatUnsignedHex(uint128_t value)
        {
            return "0x" + uint128ToHex(value);
        }

        std::string encodeHex(const std::vector<uint8_t> &data)
        {
            static const char kHexDigits[] = "0123456789abcdef";
            std::string out;
            out.reserve(data.size() * 2);
            for (uint8_t byte : data)
            {
                out.push_back(kHexDigits[(byte >> 4) & 0x0F]);
                out.push_back(kHexDigits[byte & 0x0F]);
            }
            return out;
        }

        std::string formatFloat(double value, int precision)
        {
            if (std::isnan(value))
            {
                return "nan";
            }
            if (!std::isfinite(value))
            {
                return value < 0.0 ? "-inf" : "inf";
            }
            std::ostringstream oss;
            oss.setf(std::ios::fmtflags(0), std::ios::floatfield);
            oss << std::setprecision(precision) << value;
            return oss.str();
        }

        std::string formatTimeMicros(int hour, int minute, int second, int microseconds)
        {
            std::ostringstream oss;
            oss << std::setfill('0') << std::setw(2) << hour << ":"
                << std::setw(2) << minute << ":"
                << std::setw(2) << second;
            if (microseconds != 0)
            {
                std::ostringstream frac_stream;
                frac_stream << std::setfill('0') << std::setw(6) << microseconds;
                std::string frac = frac_stream.str();
                while (!frac.empty() && frac.back() == '0')
                {
                    frac.pop_back();
                }
                if (!frac.empty())
                {
                    oss << "." << frac;
                }
            }
            return oss.str();
        }

        TimezoneManager& timezoneManager()
        {
            return getThreadLocalTimezoneManager();
        }

        int32_t resolveTimezoneOffsetSeconds(uint16_t timezone_id, int64_t local_micros)
        {
            auto &manager = timezoneManager();
            TimezoneOffset offset = manager.getOffset(timezone_id, local_micros);
            int64_t gmt_guess = local_micros -
                                static_cast<int64_t>(offset.offset_minutes) * 60 * 1000000;
            TimezoneOffset refined = manager.getOffset(timezone_id, gmt_guess);
            if (refined.offset_minutes != offset.offset_minutes)
            {
                offset = refined;
            }
            return static_cast<int32_t>(offset.offset_minutes) * 60;
        }

        bool decodeHex(const std::string &text, std::vector<uint8_t> &out, ErrorContext *ctx)
        {
            std::string trimmed = trimAscii(text);
            if (trimmed.rfind("0x", 0) == 0 || trimmed.rfind("0X", 0) == 0)
            {
                trimmed = trimmed.substr(2);
            }
            if (trimmed.rfind("\\x", 0) == 0 || trimmed.rfind("\\X", 0) == 0)
            {
                trimmed = trimmed.substr(2);
            }
            if (trimmed.size() % 2 != 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                  "Invalid hex string length");
                return false;
            }
            out.clear();
            out.reserve(trimmed.size() / 2);
            auto hex_val = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            for (size_t i = 0; i < trimmed.size(); i += 2)
            {
                int hi = hex_val(trimmed[i]);
                int lo = hex_val(trimmed[i + 1]);
                if (hi < 0 || lo < 0)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                      "Invalid hex digit");
                    return false;
                }
                out.push_back(static_cast<uint8_t>((hi << 4) | lo));
            }
            return true;
        }

        std::string encodeBase64(const std::vector<uint8_t> &data)
        {
            static const char kBase64Chars[] =
                "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            std::string result;
            result.reserve(((data.size() + 2) / 3) * 4);

            size_t i = 0;
            while (i + 2 < data.size())
            {
                uint32_t val = (static_cast<uint32_t>(data[i]) << 16) |
                               (static_cast<uint32_t>(data[i + 1]) << 8) |
                               static_cast<uint32_t>(data[i + 2]);
                result.push_back(kBase64Chars[(val >> 18) & 0x3F]);
                result.push_back(kBase64Chars[(val >> 12) & 0x3F]);
                result.push_back(kBase64Chars[(val >> 6) & 0x3F]);
                result.push_back(kBase64Chars[val & 0x3F]);
                i += 3;
            }

            if (i + 1 == data.size())
            {
                uint32_t val = static_cast<uint32_t>(data[i]) << 16;
                result.push_back(kBase64Chars[(val >> 18) & 0x3F]);
                result.push_back(kBase64Chars[(val >> 12) & 0x3F]);
                result.push_back('=');
                result.push_back('=');
            }
            else if (i + 2 == data.size())
            {
                uint32_t val = (static_cast<uint32_t>(data[i]) << 16) |
                               (static_cast<uint32_t>(data[i + 1]) << 8);
                result.push_back(kBase64Chars[(val >> 18) & 0x3F]);
                result.push_back(kBase64Chars[(val >> 12) & 0x3F]);
                result.push_back(kBase64Chars[(val >> 6) & 0x3F]);
                result.push_back('=');
            }

            return result;
        }

        bool decodeBase64(const std::string &text, std::vector<uint8_t> &out, ErrorContext *ctx)
        {
            auto base64_val = [](char c) -> int {
                if (c >= 'A' && c <= 'Z') return c - 'A';
                if (c >= 'a' && c <= 'z') return c - 'a' + 26;
                if (c >= '0' && c <= '9') return c - '0' + 52;
                if (c == '+') return 62;
                if (c == '/') return 63;
                if (c == '=') return -1;
                return -2;
            };

            std::string cleaned = text;
            cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), ::isspace), cleaned.end());

            if (cleaned.size() % 4 != 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                  "Invalid base64 length");
                return false;
            }

            out.clear();
            out.reserve((cleaned.size() / 4) * 3);

            for (size_t i = 0; i < cleaned.size(); i += 4)
            {
                int v0 = base64_val(cleaned[i]);
                int v1 = base64_val(cleaned[i + 1]);
                int v2 = base64_val(cleaned[i + 2]);
                int v3 = base64_val(cleaned[i + 3]);

                if (v0 < 0 || v1 < 0 || v2 == -2 || v3 == -2)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                      "Invalid base64 character");
                    return false;
                }

                out.push_back(static_cast<uint8_t>((v0 << 2) | (v1 >> 4)));

                if (v2 >= 0)
                {
                    out.push_back(static_cast<uint8_t>(((v1 & 0x0F) << 4) | (v2 >> 2)));
                    if (v3 >= 0)
                    {
                        out.push_back(static_cast<uint8_t>(((v2 & 0x03) << 6) | v3));
                    }
                }
            }
            return true;
        }

        std::string encodeEscape(const std::vector<uint8_t> &data)
        {
            std::string result;
            result.reserve(data.size());
            for (uint8_t byte : data)
            {
                if (byte == '\\')
                {
                    result.append("\\\\");
                }
                else if (byte >= 32 && byte < 127)
                {
                    result.push_back(static_cast<char>(byte));
                }
                else
                {
                    result.push_back('\\');
                    result.push_back('0' + ((byte >> 6) & 0x07));
                    result.push_back('0' + ((byte >> 3) & 0x07));
                    result.push_back('0' + (byte & 0x07));
                }
            }
            return result;
        }

        bool decodeEscape(const std::string &text, std::vector<uint8_t> &out, ErrorContext *ctx)
        {
            out.clear();
            out.reserve(text.size());
            for (size_t i = 0; i < text.size(); ++i)
            {
                if (text[i] == '\\' && i + 1 < text.size())
                {
                    if (text[i + 1] == '\\')
                    {
                        out.push_back('\\');
                        ++i;
                    }
                    else if (i + 3 < text.size() &&
                             text[i + 1] >= '0' && text[i + 1] <= '3' &&
                             text[i + 2] >= '0' && text[i + 2] <= '7' &&
                             text[i + 3] >= '0' && text[i + 3] <= '7')
                    {
                        uint8_t val = static_cast<uint8_t>(
                            ((text[i + 1] - '0') << 6) |
                            ((text[i + 2] - '0') << 3) |
                            (text[i + 3] - '0'));
                        out.push_back(val);
                        i += 3;
                    }
                    else
                    {
                        out.push_back(static_cast<uint8_t>(text[i]));
                    }
                }
                else
                {
                    out.push_back(static_cast<uint8_t>(text[i]));
                }
            }
            return true;
        }

        std::string formatUuid(const std::vector<uint8_t> &bytes)
        {
            if (bytes.size() != 16)
            {
                return "";
            }
            std::ostringstream oss;
            oss << std::hex << std::setfill('0')
                << std::setw(2) << static_cast<int>(bytes[0])
                << std::setw(2) << static_cast<int>(bytes[1])
                << std::setw(2) << static_cast<int>(bytes[2])
                << std::setw(2) << static_cast<int>(bytes[3]) << "-"
                << std::setw(2) << static_cast<int>(bytes[4])
                << std::setw(2) << static_cast<int>(bytes[5]) << "-"
                << std::setw(2) << static_cast<int>(bytes[6])
                << std::setw(2) << static_cast<int>(bytes[7]) << "-"
                << std::setw(2) << static_cast<int>(bytes[8])
                << std::setw(2) << static_cast<int>(bytes[9]) << "-"
                << std::setw(2) << static_cast<int>(bytes[10])
                << std::setw(2) << static_cast<int>(bytes[11])
                << std::setw(2) << static_cast<int>(bytes[12])
                << std::setw(2) << static_cast<int>(bytes[13])
                << std::setw(2) << static_cast<int>(bytes[14])
                << std::setw(2) << static_cast<int>(bytes[15]);
            return oss.str();
        }

        bool readBytes(const std::vector<uint8_t> &data, size_t &offset, size_t length,
                       std::vector<uint8_t> &out)
        {
            if (offset + length > data.size())
            {
                return false;
            }
            out.assign(data.begin() + offset, data.begin() + offset + length);
            offset += length;
            return true;
        }

        void appendPoint(std::vector<uint8_t> &out, const Point &point)
        {
            appendInt32(out, point.srid);
            appendDouble(out, point.x);
            appendDouble(out, point.y);
        }

        bool readPoint(const std::vector<uint8_t> &data, size_t &offset, Point &point)
        {
            int32_t srid = 0;
            double x = 0.0;
            double y = 0.0;
            if (!readInt32(data, offset, srid) ||
                !readDouble(data, offset, x) ||
                !readDouble(data, offset, y))
            {
                return false;
            }
            point = Point(x, y, srid);
            return true;
        }

        void appendPointList(std::vector<uint8_t> &out, const std::vector<Point> &points)
        {
            appendUint32(out, static_cast<uint32_t>(points.size()));
            for (const auto &pt : points)
            {
                appendPoint(out, pt);
            }
        }

        bool readPointList(const std::vector<uint8_t> &data, size_t &offset,
                           std::vector<Point> &points_out)
        {
            uint32_t count = 0;
            if (!readUint32(data, offset, count))
            {
                return false;
            }
            points_out.clear();
            points_out.reserve(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                Point pt;
                if (!readPoint(data, offset, pt))
                {
                    return false;
                }
                points_out.push_back(std::move(pt));
            }
            return true;
        }

        Status serializeEncryptedRecord(const EncryptedValue &encrypted,
                                        uint32_t key_version,
                                        std::vector<uint8_t> &record_out,
                                        ErrorContext *ctx)
        {
            if (encrypted.iv.size() > std::numeric_limits<uint16_t>::max() ||
                encrypted.auth_tag.size() > std::numeric_limits<uint16_t>::max() ||
                encrypted.ciphertext.size() > std::numeric_limits<uint32_t>::max())
            {
                SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Encrypted value too large");
                return Status::OUT_OF_RANGE;
            }

            record_out.clear();
            record_out.reserve(1 + 4 + 2 + 2 + 4 +
                               encrypted.iv.size() +
                               encrypted.auth_tag.size() +
                               encrypted.ciphertext.size());

            appendUint8(record_out, static_cast<uint8_t>(encrypted.algorithm));
            appendUint32(record_out, key_version);
            appendUint16(record_out, static_cast<uint16_t>(encrypted.iv.size()));
            appendUint16(record_out, static_cast<uint16_t>(encrypted.auth_tag.size()));
            appendUint32(record_out, static_cast<uint32_t>(encrypted.ciphertext.size()));

            record_out.insert(record_out.end(), encrypted.iv.begin(), encrypted.iv.end());
            record_out.insert(record_out.end(), encrypted.auth_tag.begin(), encrypted.auth_tag.end());
            record_out.insert(record_out.end(), encrypted.ciphertext.begin(), encrypted.ciphertext.end());

            return Status::OK;
        }

        Status parseEncryptedRecord(const std::vector<uint8_t> &record,
                                    EncryptedValue &encrypted_out,
                                    uint32_t &key_version_out,
                                    ErrorContext *ctx)
        {
            size_t offset = 0;
            uint8_t algo = 0;
            uint16_t iv_len = 0;
            uint16_t tag_len = 0;
            uint32_t ciphertext_len = 0;

            if (!readUint8(record, offset, algo) ||
                !readUint32(record, offset, key_version_out) ||
                !readUint16(record, offset, iv_len) ||
                !readUint16(record, offset, tag_len) ||
                !readUint32(record, offset, ciphertext_len))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid encrypted value record");
                return Status::DATA_CORRUPTED;
            }

            size_t expected_size = offset +
                                   static_cast<size_t>(iv_len) +
                                   static_cast<size_t>(tag_len) +
                                   static_cast<size_t>(ciphertext_len);
            if (expected_size != record.size())
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Encrypted value size mismatch");
                return Status::DATA_CORRUPTED;
            }

            encrypted_out.algorithm = static_cast<EncryptionAlgorithm>(algo);
            encrypted_out.iv.assign(record.begin() + offset,
                                    record.begin() + offset + iv_len);
            offset += iv_len;
            encrypted_out.auth_tag.assign(record.begin() + offset,
                                          record.begin() + offset + tag_len);
            offset += tag_len;
            encrypted_out.ciphertext.assign(record.begin() + offset,
                                            record.begin() + offset + ciphertext_len);
            encrypted_out.key_version = key_version_out;

            return Status::OK;
        }
    } // namespace

    // Define ComplexData structure for network, range, and text search types
    // ===== Constructors and Destructor =====

    TypedValue::TypedValue()
        : type_(DataType::NULL_TYPE), is_null_(true), data_{}
    {
    }

    TypedValue::TypedValue(DataType type)
        : type_(type), is_null_(true), data_{}
    {
    }

    TypedValue::TypedValue(const TypedValue& other)
        : type_(other.type_), is_null_(other.is_null_), data_{}
    {
        copyFrom(other);
    }

    TypedValue::TypedValue(TypedValue&& other) noexcept
        : type_(other.type_), is_null_(other.is_null_), data_{}
    {
        moveFrom(std::move(other));
    }

    TypedValue& TypedValue::operator=(const TypedValue& other)
    {
        if (this != &other)
        {
            clear();
            type_ = other.type_;
            is_null_ = other.is_null_;
            copyFrom(other);
        }
        return *this;
    }

    TypedValue& TypedValue::operator=(TypedValue&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            type_ = other.type_;
            is_null_ = other.is_null_;
            moveFrom(std::move(other));
        }
        return *this;
    }

    TypedValue::~TypedValue()
    {
        clear();
    }

    // ===== Factory Methods =====

    TypedValue TypedValue::makeNull(DataType type)
    {
        return TypedValue(type);
    }

    TypedValue TypedValue::makeInt32(int32_t value)
    {
        TypedValue tv(DataType::INT32);
        tv.is_null_ = false;
        tv.data_.int32_val = value;
        return tv;
    }

    TypedValue TypedValue::makeInt64(int64_t value)
    {
        TypedValue tv(DataType::INT64);
        tv.is_null_ = false;
        tv.data_.int64_val = value;
        return tv;
    }

    TypedValue TypedValue::makeInt8(int8_t value)
    {
        TypedValue tv(DataType::INT8);
        tv.is_null_ = false;
        tv.data_.int8_val = value;
        return tv;
    }

    TypedValue TypedValue::makeInt16(int16_t value)
    {
        TypedValue tv(DataType::INT16);
        tv.is_null_ = false;
        tv.data_.int16_val = value;
        return tv;
    }

    TypedValue TypedValue::makeUInt8(uint8_t value)
    {
        TypedValue tv(DataType::UINT8);
        tv.is_null_ = false;
        tv.data_.uint8_val = value;
        return tv;
    }

    TypedValue TypedValue::makeUInt16(uint16_t value)
    {
        TypedValue tv(DataType::UINT16);
        tv.is_null_ = false;
        tv.data_.uint16_val = value;
        return tv;
    }

    TypedValue TypedValue::makeUInt32(uint32_t value)
    {
        TypedValue tv(DataType::UINT32);
        tv.is_null_ = false;
        tv.data_.uint32_val = value;
        return tv;
    }

    TypedValue TypedValue::makeUInt64(uint64_t value)
    {
        TypedValue tv(DataType::UINT64);
        tv.is_null_ = false;
        tv.data_.uint64_val = value;
        return tv;
    }

    TypedValue TypedValue::makeUInt128(const std::vector<uint8_t>& value)
    {
        TypedValue tv(DataType::UINT128);
        tv.is_null_ = false;
        tv.binary_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeFloat32(float value)
    {
        TypedValue tv(DataType::FLOAT32);
        tv.is_null_ = false;
        tv.data_.float32_val = value;
        return tv;
    }

    TypedValue TypedValue::makeFloat64(double value)
    {
        TypedValue tv(DataType::FLOAT64);
        tv.is_null_ = false;
        tv.data_.float64_val = value;
        return tv;
    }

    TypedValue TypedValue::makeBool(bool value)
    {
        TypedValue tv(DataType::BOOLEAN);
        tv.is_null_ = false;
        tv.data_.bool_val = value;
        return tv;
    }

    TypedValue TypedValue::makeVarchar(const std::string& value)
    {
        TypedValue tv(DataType::VARCHAR);
        tv.is_null_ = false;
        tv.string_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeText(const std::string& value)
    {
        TypedValue tv(DataType::TEXT);
        tv.is_null_ = false;
        tv.string_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeJSON(const std::string& value)
    {
        TypedValue tv(DataType::JSON);
        tv.is_null_ = false;
        tv.string_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeJSONB(const std::vector<uint8_t>& value)
    {
        TypedValue tv(DataType::JSONB);
        tv.is_null_ = false;
        tv.binary_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeChar(const std::string& value)
    {
        TypedValue tv(DataType::CHAR);
        tv.is_null_ = false;
        tv.string_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeBinary(const std::vector<uint8_t>& value)
    {
        TypedValue tv(DataType::BINARY);
        tv.is_null_ = false;
        tv.binary_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeVarbinary(const std::vector<uint8_t>& value)
    {
        TypedValue tv(DataType::VARBINARY);
        tv.is_null_ = false;
        tv.binary_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeBlob(const std::vector<uint8_t>& value)
    {
        TypedValue tv(DataType::BLOB);
        tv.is_null_ = false;
        tv.binary_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeBytea(const std::vector<uint8_t>& value)
    {
        TypedValue tv(DataType::BYTEA);
        tv.is_null_ = false;
        tv.binary_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeXML(const std::string& value)
    {
        TypedValue tv(DataType::XML);
        tv.is_null_ = false;
        tv.string_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeDecimal(int128_t unscaled_value, uint8_t precision, uint8_t scale)
    {
        TypedValue tv(DataType::DECIMAL);
        tv.is_null_ = false;
        tv.decimal_unscaled_ = unscaled_value;
        tv.decimal_precision_ = precision;
        tv.decimal_scale_ = scale;
        return tv;
    }

    TypedValue TypedValue::makeDecfloat(const DecFloat& value)
    {
        TypedValue tv(value.precision == 16 ? DataType::DECFLOAT16 : DataType::DECFLOAT34);
        tv.is_null_ = false;
        std::vector<uint8_t> bytes;
        ErrorContext ctx;
        Status st = encodeDecfloat(value, tv.type_, bytes, &ctx);
        if (st != Status::OK)
        {
            tv.is_null_ = true;
            tv.type_ = DataType::NULL_TYPE;
            return tv;
        }
        tv.binary_data_ = std::move(bytes);
        return tv;
    }

    TypedValue TypedValue::makeDecfloat(DataType type, const std::vector<uint8_t>& bytes)
    {
        TypedValue tv(type);
        tv.is_null_ = false;
        tv.binary_data_ = bytes;
        return tv;
    }

    TypedValue TypedValue::makePoint(const Point& value)
    {
        TypedValue tv(DataType::POINT);
        tv.is_null_ = false;
        tv.spatial_data_ = std::make_unique<SpatialData>();
        tv.spatial_data_->point = value;
        return tv;
    }

    TypedValue TypedValue::makeLineString(const LineString& value)
    {
        TypedValue tv(DataType::LINESTRING);
        tv.is_null_ = false;
        tv.spatial_data_ = std::make_unique<SpatialData>();
        tv.spatial_data_->linestring = value;
        return tv;
    }

    TypedValue TypedValue::makePolygon(const Polygon& value)
    {
        TypedValue tv(DataType::POLYGON);
        tv.is_null_ = false;
        tv.spatial_data_ = std::make_unique<SpatialData>();
        tv.spatial_data_->polygon = value;
        return tv;
    }

    TypedValue TypedValue::makeMultiPoint(const MultiPoint& value)
    {
        TypedValue tv(DataType::MULTIPOINT);
        tv.is_null_ = false;
        tv.spatial_data_ = std::make_unique<SpatialData>();
        tv.spatial_data_->multipoint = value;
        return tv;
    }

    TypedValue TypedValue::makeMultiLineString(const MultiLineString& value)
    {
        TypedValue tv(DataType::MULTILINESTRING);
        tv.is_null_ = false;
        tv.spatial_data_ = std::make_unique<SpatialData>();
        tv.spatial_data_->multilinestring = value;
        return tv;
    }

    TypedValue TypedValue::makeMultiPolygon(const MultiPolygon& value)
    {
        TypedValue tv(DataType::MULTIPOLYGON);
        tv.is_null_ = false;
        tv.spatial_data_ = std::make_unique<SpatialData>();
        tv.spatial_data_->multipolygon = value;
        return tv;
    }

    TypedValue TypedValue::makeGeometryCollection(const GeometryCollection& value)
    {
        TypedValue tv(DataType::GEOMETRYCOLLECTION);
        tv.is_null_ = false;
        tv.spatial_data_ = std::make_unique<SpatialData>();
        tv.spatial_data_->geometrycollection = value;
        return tv;
    }

    // Network types
    TypedValue TypedValue::makeInet(const InetAddr& value)
    {
        TypedValue tv(DataType::INET);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->inet = std::make_unique<InetAddr>(value);
        return tv;
    }

    TypedValue TypedValue::makeCidr(const Cidr& value)
    {
        TypedValue tv(DataType::CIDR);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->cidr = std::make_unique<Cidr>(value);
        return tv;
    }

    TypedValue TypedValue::makeMacAddr(const MacAddr& value)
    {
        TypedValue tv(DataType::MACADDR);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->macaddr = std::make_unique<MacAddr>(value);
        return tv;
    }

    TypedValue TypedValue::makeMacAddr8(const MacAddr8& value)
    {
        TypedValue tv(DataType::MACADDR8);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->macaddr8 = std::make_unique<MacAddr8>(value);
        return tv;
    }

    // Text search types
    TypedValue TypedValue::makeTSVector(const TSVector& value)
    {
        TypedValue tv(DataType::TSVECTOR);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->tsvector = std::make_shared<TSVector>(value);
        return tv;
    }

    TypedValue TypedValue::makeTSVector(const std::shared_ptr<TSVector>& value)
    {
        TypedValue tv(DataType::TSVECTOR);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->tsvector = value;
        return tv;
    }

    TypedValue TypedValue::makeTSQuery(const std::shared_ptr<TSQuery>& value)
    {
        TypedValue tv(DataType::TSQUERY);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->tsquery = value;
        return tv;
    }

    // Temporal types
    TypedValue TypedValue::makeDate(int64_t days_since_epoch, int32_t offset_seconds)
    {
        TypedValue tv(DataType::DATE);
        tv.is_null_ = false;
        tv.data_.int64_val = days_since_epoch;
        tv.timezone_offset_seconds_ = offset_seconds;
        return tv;
    }

    TypedValue TypedValue::makeTime(int64_t microseconds, int32_t offset_seconds)
    {
        TypedValue tv(DataType::TIME);
        tv.is_null_ = false;
        tv.data_.int64_val = microseconds;
        tv.timezone_offset_seconds_ = offset_seconds;
        return tv;
    }

    TypedValue TypedValue::makeTimestamp(int64_t microseconds_since_epoch, int32_t offset_seconds)
    {
        TypedValue tv(DataType::TIMESTAMP);
        tv.is_null_ = false;
        tv.data_.int64_val = microseconds_since_epoch;
        tv.timezone_offset_seconds_ = offset_seconds;
        return tv;
    }

    // Other types
    TypedValue TypedValue::makeUUID(const std::vector<uint8_t>& value)
    {
        TypedValue tv(DataType::UUID);
        tv.is_null_ = false;
        tv.binary_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeInterval(const Interval& value)
    {
        TypedValue tv(DataType::INTERVAL);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->interval = std::make_unique<Interval>(value);
        return tv;
    }

    TypedValue TypedValue::makeArray(const std::vector<TypedValue>& elements)
    {
        TypedValue tv(DataType::ARRAY);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->array = std::make_unique<std::vector<TypedValue>>(elements);
        return tv;
    }

    TypedValue TypedValue::makeInt128(const std::vector<uint8_t>& value)
    {
        TypedValue tv(DataType::INT128);
        tv.is_null_ = false;
        tv.binary_data_ = value;
        return tv;
    }

    TypedValue TypedValue::makeMoney(int64_t value)
    {
        TypedValue tv(DataType::MONEY);
        tv.is_null_ = false;
        tv.data_.int64_val = value;
        return tv;
    }

    TypedValue TypedValue::makeVector(const std::vector<float>& value)
    {
        TypedValue tv(DataType::VECTOR);
        tv.is_null_ = false;
        // Store as binary data (convert floats to bytes)
        tv.binary_data_.resize(value.size() * sizeof(float));
        std::memcpy(tv.binary_data_.data(), value.data(), value.size() * sizeof(float));
        return tv;
    }

    TypedValue TypedValue::makeInt4Range(const Range<int32_t>& value)
    {
        TypedValue tv(DataType::INT4RANGE);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->range_data = std::make_shared<Range<int32_t>>(value);
        return tv;
    }

    TypedValue TypedValue::makeInt8Range(const Range<int64_t>& value)
    {
        TypedValue tv(DataType::INT8RANGE);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->range_data = std::make_shared<Range<int64_t>>(value);
        return tv;
    }

    TypedValue TypedValue::makeNumRange(const Range<double>& value)
    {
        TypedValue tv(DataType::NUMRANGE);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->range_data = std::make_shared<Range<double>>(value);
        return tv;
    }

    TypedValue TypedValue::makeComposite(const std::vector<std::string>& field_names, const std::vector<TypedValue>& field_values)
    {
        if (field_names.size() != field_values.size()) {
            throw std::runtime_error("Field names and values count mismatch");
        }
        TypedValue tv(DataType::COMPOSITE);
        tv.is_null_ = false;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->array = std::make_unique<std::vector<TypedValue>>(field_values);
        // Store field names in string_data as a serialized format
        std::string names_str;
        for (const auto& name : field_names) {
            names_str.append(name);
            names_str.push_back('\0');
        }
        tv.string_data_ = names_str;
        return tv;
    }

    TypedValue TypedValue::makeVariant(const TypedValue& value)
    {
        TypedValue tv(DataType::VARIANT);
        tv.is_null_ = value.is_null_;
        // Store the wrapped value's type and data
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->array = std::make_unique<std::vector<TypedValue>>();
        tv.complex_data_->array->push_back(value);
        return tv;
    }

    TypedValue TypedValue::makeVariant(DataType type, const TypedValue& value)
    {
        TypedValue tv(DataType::VARIANT);
        tv.is_null_ = value.is_null_;
        tv.complex_data_ = std::make_unique<ComplexData>();
        tv.complex_data_->array = std::make_unique<std::vector<TypedValue>>();
        // Store type information as first element
        TypedValue type_val(DataType::INT32);
        type_val.data_.int32_val = static_cast<int32_t>(type);
        type_val.is_null_ = false;
        tv.complex_data_->array->push_back(type_val);
        tv.complex_data_->array->push_back(value);
        return tv;
    }

    // ===== Getters =====

    int32_t TypedValue::getInt32() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        switch (type_)
        {
            case DataType::INT8:
                return static_cast<int32_t>(data_.int8_val);
            case DataType::INT16:
                return static_cast<int32_t>(data_.int16_val);
            case DataType::INT32:
                return data_.int32_val;
            default:
                throw std::runtime_error("Type mismatch: expected INT32");
        }
    }

    int64_t TypedValue::getInt64() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        switch (type_)
        {
            case DataType::INT8:
                return static_cast<int64_t>(data_.int8_val);
            case DataType::INT16:
                return static_cast<int64_t>(data_.int16_val);
            case DataType::INT32:
                return static_cast<int64_t>(data_.int32_val);
            case DataType::INT64:
                return data_.int64_val;
            default:
                throw std::runtime_error("Type mismatch: expected INT64");
        }
    }

    uint8_t TypedValue::getUInt8() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::UINT8) {
            throw std::runtime_error("Type mismatch: expected UINT8");
        }
        return data_.uint8_val;
    }

    uint16_t TypedValue::getUInt16() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::UINT16) {
            throw std::runtime_error("Type mismatch: expected UINT16");
        }
        return data_.uint16_val;
    }

    uint32_t TypedValue::getUInt32() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::UINT32) {
            throw std::runtime_error("Type mismatch: expected UINT32");
        }
        return data_.uint32_val;
    }

    uint64_t TypedValue::getUInt64() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::UINT64) {
            throw std::runtime_error("Type mismatch: expected UINT64");
        }
        return data_.uint64_val;
    }

    float TypedValue::getFloat32() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::FLOAT32) {
            throw std::runtime_error("Type mismatch: expected FLOAT32");
        }
        return data_.float32_val;
    }

    double TypedValue::getFloat64() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::FLOAT64) {
            throw std::runtime_error("Type mismatch: expected FLOAT64");
        }
        return data_.float64_val;
    }

    bool TypedValue::getBool() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::BOOLEAN) {
            throw std::runtime_error("Type mismatch: expected BOOLEAN");
        }
        return data_.bool_val;
    }

    std::string TypedValue::getVarchar() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::VARCHAR) {
            throw std::runtime_error("Type mismatch: expected VARCHAR");
        }
        return string_data_;
    }

    std::string TypedValue::getText() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::TEXT) {
            throw std::runtime_error("Type mismatch: expected TEXT");
        }
        return string_data_;
    }

    std::string TypedValue::getChar() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::CHAR) {
            throw std::runtime_error("Type mismatch: expected CHAR");
        }
        return string_data_;
    }

    Point TypedValue::getPoint() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::POINT) {
            throw std::runtime_error("Type mismatch: expected POINT");
        }
        if (!spatial_data_) {
            throw std::runtime_error("Spatial data not initialized");
        }
        return spatial_data_->point;
    }

    LineString TypedValue::getLineString() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::LINESTRING) {
            throw std::runtime_error("Type mismatch: expected LINESTRING");
        }
        if (!spatial_data_) {
            throw std::runtime_error("Spatial data not initialized");
        }
        return spatial_data_->linestring;
    }

    Polygon TypedValue::getPolygon() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::POLYGON) {
            throw std::runtime_error("Type mismatch: expected POLYGON");
        }
        if (!spatial_data_) {
            throw std::runtime_error("Spatial data not initialized");
        }
        return spatial_data_->polygon;
    }

    MultiPoint TypedValue::getMultiPoint() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::MULTIPOINT) {
            throw std::runtime_error("Type mismatch: expected MULTIPOINT");
        }
        if (!spatial_data_) {
            throw std::runtime_error("Spatial data not initialized");
        }
        return spatial_data_->multipoint;
    }

    MultiLineString TypedValue::getMultiLineString() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::MULTILINESTRING) {
            throw std::runtime_error("Type mismatch: expected MULTILINESTRING");
        }
        if (!spatial_data_) {
            throw std::runtime_error("Spatial data not initialized");
        }
        return spatial_data_->multilinestring;
    }

    MultiPolygon TypedValue::getMultiPolygon() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::MULTIPOLYGON) {
            throw std::runtime_error("Type mismatch: expected MULTIPOLYGON");
        }
        if (!spatial_data_) {
            throw std::runtime_error("Spatial data not initialized");
        }
        return spatial_data_->multipolygon;
    }

    const GeometryCollection& TypedValue::getGeometryCollection() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::GEOMETRYCOLLECTION) {
            throw std::runtime_error("Type mismatch: expected GEOMETRYCOLLECTION");
        }
        if (!spatial_data_) {
            throw std::runtime_error("Spatial data not initialized");
        }
        return spatial_data_->geometrycollection;
    }

    // Network types
    const InetAddr& TypedValue::getInet() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::INET) {
            throw std::runtime_error("Type mismatch: expected INET");
        }
        if (!complex_data_ || !complex_data_->inet) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->inet;
    }

    const Cidr& TypedValue::getCidr() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::CIDR) {
            throw std::runtime_error("Type mismatch: expected CIDR");
        }
        if (!complex_data_ || !complex_data_->cidr) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->cidr;
    }

    const MacAddr& TypedValue::getMacAddr() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::MACADDR) {
            throw std::runtime_error("Type mismatch: expected MACADDR");
        }
        if (!complex_data_ || !complex_data_->macaddr) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->macaddr;
    }

    const MacAddr8& TypedValue::getMacAddr8() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::MACADDR8) {
            throw std::runtime_error("Type mismatch: expected MACADDR8");
        }
        if (!complex_data_ || !complex_data_->macaddr8) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->macaddr8;
    }

    // Text search types
    const TSVector& TypedValue::getTSVector() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::TSVECTOR) {
            throw std::runtime_error("Type mismatch: expected TSVECTOR");
        }
        if (!complex_data_ || !complex_data_->tsvector) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->tsvector;
    }

    const TSQuery& TypedValue::getTSQuery() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::TSQUERY) {
            throw std::runtime_error("Type mismatch: expected TSQUERY");
        }
        if (!complex_data_ || !complex_data_->tsquery) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->tsquery;
    }

    // Other types
    const std::vector<uint8_t>& TypedValue::getUUID() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::UUID) {
            throw std::runtime_error("Type mismatch: expected UUID");
        }
        return binary_data_;
    }

    int128_t TypedValue::getInt128() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::INT128) {
            throw std::runtime_error("Type mismatch: expected INT128");
        }
        if (binary_data_.size() != 16) {
            throw std::runtime_error("INT128 storage size invalid");
        }
        size_t offset = 0;
        int128_t value = 0;
        std::vector<uint8_t> bytes = binary_data_;
        if (!readInt128(bytes, offset, 16, value)) {
            throw std::runtime_error("INT128 decode failed");
        }
        return value;
    }

    uint128_t TypedValue::getUInt128() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::UINT128) {
            throw std::runtime_error("Type mismatch: expected UINT128");
        }
        if (binary_data_.size() != 16) {
            throw std::runtime_error("UINT128 storage size invalid");
        }
        size_t offset = 0;
        uint128_t value = 0;
        std::vector<uint8_t> bytes = binary_data_;
        if (!readUint128(bytes, offset, 16, value)) {
            throw std::runtime_error("UINT128 decode failed");
        }
        return value;
    }

    const std::vector<uint8_t>& TypedValue::getBinary() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        switch (type_)
        {
            case DataType::BINARY:
            case DataType::VARBINARY:
            case DataType::BLOB:
            case DataType::BYTEA:
            case DataType::UUID:
            case DataType::INT128:
            case DataType::UINT128:
            case DataType::JSONB:
            case DataType::VECTOR:
                return binary_data_;
            default:
                throw std::runtime_error("Type mismatch: expected binary type");
        }
    }

    const Interval& TypedValue::getInterval() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::INTERVAL) {
            throw std::runtime_error("Type mismatch: expected INTERVAL");
        }
        if (!complex_data_ || !complex_data_->interval) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->interval;
    }

    // Temporal types
    int64_t TypedValue::getDate() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::DATE) {
            throw std::runtime_error("Type mismatch: expected DATE");
        }
        return data_.int64_val;
    }

    int64_t TypedValue::getTime() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::TIME) {
            throw std::runtime_error("Type mismatch: expected TIME");
        }
        return data_.int64_val;
    }

    int64_t TypedValue::getTimestamp() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::TIMESTAMP) {
            throw std::runtime_error("Type mismatch: expected TIMESTAMP");
        }
        return data_.int64_val;
    }

    const std::vector<TypedValue>& TypedValue::getArray() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::ARRAY) {
            throw std::runtime_error("Type mismatch: expected ARRAY");
        }
        if (!complex_data_ || !complex_data_->array) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->array;
    }

    const Range<int32_t>& TypedValue::getInt4Range() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::INT4RANGE) {
            throw std::runtime_error("Type mismatch: expected INT4RANGE");
        }
        if (!complex_data_ || !complex_data_->range_data) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *std::static_pointer_cast<Range<int32_t>>(complex_data_->range_data);
    }

    const Range<int64_t>& TypedValue::getInt8Range() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::INT8RANGE) {
            throw std::runtime_error("Type mismatch: expected INT8RANGE");
        }
        if (!complex_data_ || !complex_data_->range_data) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *std::static_pointer_cast<Range<int64_t>>(complex_data_->range_data);
    }

    const Range<double>& TypedValue::getNumRange() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::NUMRANGE) {
            throw std::runtime_error("Type mismatch: expected NUMRANGE");
        }
        if (!complex_data_ || !complex_data_->range_data) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *std::static_pointer_cast<Range<double>>(complex_data_->range_data);
    }

    const std::vector<TypedValue>& TypedValue::getCompositeValues() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::COMPOSITE) {
            throw std::runtime_error("Type mismatch: expected COMPOSITE");
        }
        if (!complex_data_ || !complex_data_->array) {
            throw std::runtime_error("Complex data not initialized");
        }
        return *complex_data_->array;
    }

    std::vector<std::string> TypedValue::getCompositeFieldNames() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::COMPOSITE) {
            throw std::runtime_error("Type mismatch: expected COMPOSITE");
        }
        std::vector<std::string> names;
        if (string_data_.empty()) {
            return names;
        }
        size_t start = 0;
        while (start < string_data_.size()) {
            size_t end = string_data_.find('\0', start);
            if (end == std::string::npos) {
                end = string_data_.size();
            }
            if (end > start) {
                names.emplace_back(string_data_.substr(start, end - start));
            } else {
                names.emplace_back("");
            }
            start = end + 1;
        }
        return names;
    }

    const TypedValue& TypedValue::getVariantValue() const
    {
        if (is_null_) {
            throw std::runtime_error("Cannot get value from NULL");
        }
        ensureDecrypted();
        if (type_ != DataType::VARIANT) {
            throw std::runtime_error("Type mismatch: expected VARIANT");
        }
        if (!complex_data_ || !complex_data_->array || complex_data_->array->empty()) {
            throw std::runtime_error("Variant payload not initialized");
        }
        const auto& payload = *complex_data_->array;
        if (payload.size() >= 2 && payload[0].type() == DataType::INT32) {
            return payload[1];
        }
        return payload[0];
    }

    std::optional<DataType> TypedValue::getVariantTag() const
    {
        if (is_null_) {
            return std::nullopt;
        }
        ensureDecrypted();
        if (type_ != DataType::VARIANT) {
            throw std::runtime_error("Type mismatch: expected VARIANT");
        }
        if (!complex_data_ || !complex_data_->array || complex_data_->array->empty()) {
            throw std::runtime_error("Variant payload not initialized");
        }
        const auto& payload = *complex_data_->array;
        if (payload.size() >= 2 && payload[0].type() == DataType::INT32) {
            int32_t type_code = payload[0].getInt32();
            return static_cast<DataType>(type_code);
        }
        return std::nullopt;
    }

    // toString method
    std::string TypedValue::toString() const
    {
        if (is_null_) {
            return "NULL";
        }
        ensureDecrypted();

        switch (type_) {
            case DataType::INT8:
                return std::to_string(data_.int8_val);
            case DataType::INT16:
                return std::to_string(data_.int16_val);
            case DataType::INT32:
                return std::to_string(data_.int32_val);
            case DataType::INT64:
                return std::to_string(data_.int64_val);
            case DataType::UINT8:
                return std::to_string(data_.uint8_val);
            case DataType::UINT16:
                return std::to_string(data_.uint16_val);
            case DataType::UINT32:
                return std::to_string(data_.uint32_val);
            case DataType::UINT64:
                return std::to_string(data_.uint64_val);
            case DataType::FLOAT32:
                return formatFloat(static_cast<double>(data_.float32_val),
                                   std::numeric_limits<float>::max_digits10);
            case DataType::FLOAT64:
                return formatFloat(data_.float64_val,
                                   std::numeric_limits<double>::max_digits10);
            case DataType::DECIMAL:
            {
                uint8_t precision = decimal_precision_ == 0 ? DECIMAL_MAX_PRECISION : decimal_precision_;
                Decimal decimal(decimal_unscaled_, precision, decimal_scale_);
                return decimal.toStringWithPrecision(decimal_scale_);
            }
            case DataType::DECFLOAT16:
            case DataType::DECFLOAT34:
            {
                DecFloat value;
                ErrorContext ctx;
                Status st = decodeDecfloat(binary_data_, type_, value, &ctx);
                if (st != Status::OK)
                {
                    return "<DECFLOAT>";
                }
                return value.toString();
            }
            case DataType::MONEY:
            {
                Decimal decimal(static_cast<int128_t>(data_.int64_val), 19, 4);
                return decimal.toStringWithPrecision(4);
            }
            case DataType::BOOLEAN:
                return data_.bool_val ? "true" : "false";
            case DataType::VARCHAR:
            case DataType::TEXT:
            case DataType::CHAR:
            case DataType::JSON:
            case DataType::XML:
                return string_data_;
            case DataType::JSONB:
            {
                if (binary_data_.empty())
                {
                    return string_data_;
                }
                std::string decoded;
                if (!decodeJsonb(binary_data_, decoded))
                {
                    return string_data_;
                }
                return decoded;
            }
            case DataType::DATE:
            {
                int64_t days = data_.int64_val;
                int32_t offset = timezone_offset_seconds_;
                int64_t default_micros = defaultDateTimeMicros();
                int64_t day_micros =
                    static_cast<int64_t>(FirebirdDateTime::SECONDS_PER_DAY) * 1000000;
                int64_t adjust = floorDiv(default_micros -
                                              static_cast<int64_t>(offset) * 1000000,
                                          day_micros);
                int64_t local_days = days - adjust;
                int32_t mjd_date =
                    static_cast<int32_t>(local_days + FirebirdDateTime::UNIX_EPOCH_MJD);
                std::string result = FirebirdDateTime::formatDate(mjd_date);
                if (offset != 0)
                {
                    result += formatOffsetSeconds(offset);
                }
                return result;
            }
            case DataType::TIME:
            {
                int64_t micros = data_.int64_val;
                int32_t offset = timezone_offset_seconds_;
                int64_t local_micros = micros + static_cast<int64_t>(offset) * 1000000;
                const int64_t micros_per_day =
                    static_cast<int64_t>(FirebirdDateTime::SECONDS_PER_DAY) * 1000000;
                local_micros %= micros_per_day;
                if (local_micros < 0)
                {
                    local_micros += micros_per_day;
                }
                int64_t total_seconds = local_micros / 1000000;
                int32_t micro_remainder = static_cast<int32_t>(local_micros % 1000000);
                if (micro_remainder < 0)
                {
                    micro_remainder += 1000000;
                    total_seconds -= 1;
                }
                int hour = static_cast<int>(total_seconds / 3600);
                int minute = static_cast<int>((total_seconds / 60) % 60);
                int second = static_cast<int>(total_seconds % 60);
                std::string result = formatTimeMicros(hour, minute, second, micro_remainder);
                if (offset != 0)
                {
                    result += formatOffsetSeconds(offset);
                }
                return result;
            }
            case DataType::TIMESTAMP:
            {
                int64_t utc_micros = data_.int64_val;
                int32_t offset = timezone_offset_seconds_;
                int64_t local_micros = utc_micros + static_cast<int64_t>(offset) * 1000000;
                int64_t total_seconds = floorDiv(local_micros, 1000000);
                int64_t micro_remainder = local_micros - total_seconds * 1000000;
                if (micro_remainder < 0)
                {
                    micro_remainder += 1000000;
                    total_seconds -= 1;
                }
                int64_t days = floorDiv(total_seconds, FirebirdDateTime::SECONDS_PER_DAY);
                int64_t seconds_of_day = total_seconds -
                                         days * FirebirdDateTime::SECONDS_PER_DAY;
                if (seconds_of_day < 0)
                {
                    seconds_of_day += FirebirdDateTime::SECONDS_PER_DAY;
                    days -= 1;
                }
                int hour = static_cast<int>(seconds_of_day / 3600);
                int minute = static_cast<int>((seconds_of_day / 60) % 60);
                int second = static_cast<int>(seconds_of_day % 60);
                int32_t mjd_date = static_cast<int32_t>(days + FirebirdDateTime::UNIX_EPOCH_MJD);
                std::string result = FirebirdDateTime::formatDate(mjd_date) + " " +
                                     formatTimeMicros(hour, minute, second,
                                                      static_cast<int32_t>(micro_remainder));
                if (offset != 0)
                {
                    result += formatOffsetSeconds(offset);
                }
                return result;
            }
            case DataType::UUID:
            {
                std::string formatted = formatUuid(binary_data_);
                return formatted.empty() ? "<UUID>" : formatted;
            }
            case DataType::INT128:
            {
                if (binary_data_.size() != 16)
                {
                    return "<INT128>";
                }
                int128_t value = 0;
                size_t offset = 0;
                std::vector<uint8_t> bytes = binary_data_;
                if (!readInt128(bytes, offset, 16, value))
                {
                    return "<INT128>";
                }
                return int128ToString(value);
            }
            case DataType::UINT128:
            {
                if (binary_data_.size() != 16)
                {
                    return "<UINT128>";
                }
                uint128_t value = 0;
                size_t offset = 0;
                std::vector<uint8_t> bytes = binary_data_;
                if (!readUint128(bytes, offset, 16, value))
                {
                    return "<UINT128>";
                }
                return uint128ToString(value);
            }
            case DataType::BINARY:
            case DataType::VARBINARY:
            case DataType::BLOB:
            case DataType::BYTEA:
                return encodeHex(binary_data_);
            case DataType::VECTOR:
            {
                if (binary_data_.size() % sizeof(float) != 0)
                {
                    return encodeHex(binary_data_);
                }
                size_t count = binary_data_.size() / sizeof(float);
                std::ostringstream oss;
                oss << "[";
                for (size_t i = 0; i < count; ++i)
                {
                    float val = 0.0f;
                    std::memcpy(&val, binary_data_.data() + i * sizeof(float), sizeof(float));
                    if (i > 0)
                    {
                        oss << ", ";
                    }
                    oss << val;
                }
                oss << "]";
                return oss.str();
            }
            case DataType::POINT:
                if (spatial_data_)
                {
                    return spatial::WKTParser::pointToWKT(spatial_data_->point);
                }
                return "POINT EMPTY";
            case DataType::LINESTRING:
                if (spatial_data_)
                {
                    return spatial::WKTParser::lineStringToWKT(spatial_data_->linestring);
                }
                return "LINESTRING EMPTY";
            case DataType::POLYGON:
                if (spatial_data_)
                {
                    return spatial::WKTParser::polygonToWKT(spatial_data_->polygon);
                }
                return "POLYGON EMPTY";
            case DataType::MULTIPOINT:
                if (spatial_data_)
                {
                    const auto& mp = spatial_data_->multipoint;
                    std::ostringstream oss;
                    oss << "MULTIPOINT(";
                    for (size_t i = 0; i < mp.points.size(); ++i)
                    {
                        if (i > 0)
                        {
                            oss << ", ";
                        }
                        oss << "(" << mp.points[i].x << " " << mp.points[i].y << ")";
                    }
                    oss << ")";
                    return oss.str();
                }
                return "MULTIPOINT EMPTY";
            case DataType::MULTILINESTRING:
                if (spatial_data_)
                {
                    const auto& mls = spatial_data_->multilinestring;
                    std::ostringstream oss;
                    oss << "MULTILINESTRING(";
                    for (size_t i = 0; i < mls.linestrings.size(); ++i)
                    {
                        if (i > 0)
                        {
                            oss << ", ";
                        }
                        oss << "(";
                        const auto& ls = mls.linestrings[i];
                        for (size_t j = 0; j < ls.points.size(); ++j)
                        {
                            if (j > 0)
                            {
                                oss << ", ";
                            }
                            oss << ls.points[j].x << " " << ls.points[j].y;
                        }
                        oss << ")";
                    }
                    oss << ")";
                    return oss.str();
                }
                return "MULTILINESTRING EMPTY";
            case DataType::MULTIPOLYGON:
                if (spatial_data_)
                {
                    const auto& mpoly = spatial_data_->multipolygon;
                    std::ostringstream oss;
                    oss << "MULTIPOLYGON(";
                    for (size_t i = 0; i < mpoly.polygons.size(); ++i)
                    {
                        if (i > 0)
                        {
                            oss << ", ";
                        }
                        oss << "(";
                        const auto& poly = mpoly.polygons[i];
                        for (size_t j = 0; j < poly.rings.size(); ++j)
                        {
                            if (j > 0)
                            {
                                oss << ", ";
                            }
                            oss << "(";
                            const auto& ring = poly.rings[j];
                            for (size_t k = 0; k < ring.size(); ++k)
                            {
                                if (k > 0)
                                {
                                    oss << ", ";
                                }
                                oss << ring[k].x << " " << ring[k].y;
                            }
                            oss << ")";
                        }
                        oss << ")";
                    }
                    oss << ")";
                    return oss.str();
                }
                return "MULTIPOLYGON EMPTY";
            case DataType::GEOMETRYCOLLECTION:
                if (spatial_data_)
                {
                    const auto& gc = spatial_data_->geometrycollection;
                    std::ostringstream oss;
                    oss << "GEOMETRYCOLLECTION(";
                    for (size_t i = 0; i < gc.geometries.size(); ++i)
                    {
                        if (i > 0)
                        {
                            oss << ", ";
                        }
                        const auto& geom = gc.geometries[i];
                        if (geom)
                        {
                            oss << geom->toString();
                        }
                    }
                    oss << ")";
                    return oss.str();
                }
                return "GEOMETRYCOLLECTION EMPTY";
            case DataType::INET:
                if (complex_data_ && complex_data_->inet) {
                    return complex_data_->inet->toString();
                }
                return "INET(?)";
            case DataType::CIDR:
                if (complex_data_ && complex_data_->cidr) {
                    return complex_data_->cidr->toString();
                }
                return "CIDR(?)";
            case DataType::MACADDR:
                if (complex_data_ && complex_data_->macaddr) {
                    return complex_data_->macaddr->toString();
                }
                return "MACADDR(?)";
            case DataType::MACADDR8:
                if (complex_data_ && complex_data_->macaddr8) {
                    return complex_data_->macaddr8->toString();
                }
                return "MACADDR8(?)";
            case DataType::DATERANGE:
            case DataType::TSRANGE:
            case DataType::TSTZRANGE:
                if (complex_data_ && complex_data_->range_data) {
                    auto* range = static_cast<Range<int64_t>*>(complex_data_->range_data.get());
                    return range->toString();
                }
                return "<empty range>";
            case DataType::INT4RANGE:
                if (complex_data_ && complex_data_->range_data) {
                    auto* range = static_cast<Range<int32_t>*>(complex_data_->range_data.get());
                    return range->toString();
                }
                return "<empty range>";
            case DataType::INT8RANGE:
                if (complex_data_ && complex_data_->range_data) {
                    auto* range = static_cast<Range<int64_t>*>(complex_data_->range_data.get());
                    return range->toString();
                }
                return "<empty range>";
            case DataType::NUMRANGE:
                if (complex_data_ && complex_data_->range_data) {
                    auto* range = static_cast<Range<double>*>(complex_data_->range_data.get());
                    return range->toString();
                }
                return "<empty range>";
            case DataType::TSVECTOR:
                if (complex_data_ && complex_data_->tsvector) {
                    return complex_data_->tsvector->toString();
                }
                return "<empty tsvector>";
            case DataType::TSQUERY:
                if (complex_data_ && complex_data_->tsquery) {
                    return complex_data_->tsquery->toString();
                }
                return "<empty tsquery>";
            case DataType::INTERVAL:
                if (complex_data_ && complex_data_->interval) {
                    const auto& interval = *complex_data_->interval;
                    return "interval " + std::to_string(interval.months) + " " +
                        std::to_string(interval.days) + " " +
                        std::to_string(interval.microseconds);
                }
                return "<empty interval>";
            case DataType::ARRAY:
            case DataType::COMPOSITE:
                if (complex_data_ && complex_data_->array) {
                    std::ostringstream oss;
                    oss << (type_ == DataType::ARRAY ? "{" : "(");
                    for (size_t i = 0; i < complex_data_->array->size(); ++i)
                    {
                        if (i > 0)
                        {
                            oss << ", ";
                        }
                        oss << (*complex_data_->array)[i].toString();
                    }
                    oss << (type_ == DataType::ARRAY ? "}" : ")");
                    return oss.str();
                }
                return type_ == DataType::ARRAY ? "<empty array>" : "<empty composite>";
            case DataType::VARIANT:
                if (complex_data_ && complex_data_->array && !complex_data_->array->empty()) {
                    if (complex_data_->array->size() == 1)
                    {
                        return (*complex_data_->array)[0].toString();
                    }
                    std::ostringstream oss;
                    oss << "{";
                    for (size_t i = 0; i < complex_data_->array->size(); ++i)
                    {
                        if (i > 0)
                        {
                            oss << ", ";
                        }
                        oss << (*complex_data_->array)[i].toString();
                    }
                    oss << "}";
                    return oss.str();
                }
                return "<empty variant>";
            default:
                return "<" + std::string(TypeSystem::getTypeName(type_)) + ">";
        }
    }

    Status TypedValue::encrypt(const std::vector<uint8_t>& key,
                               EncryptionAlgorithm algo,
                               uint32_t key_version,
                               ErrorContext* ctx)
    {
        if (is_null_) {
            return Status::OK;
        }

        if (is_encrypted_) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Value is already encrypted");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<uint8_t> plaintext;
        Status status = serializePlainValue(plaintext, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        EncryptedValue encrypted;
        status = DataEncryption::encrypt(plaintext, key, algo, encrypted, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint8_t> record;
        status = serializeEncryptedRecord(encrypted, key_version, record, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Clear plaintext payload
        string_data_.clear();
        binary_data_.clear();
        spatial_data_.reset();
        complex_data_.reset();
        std::memset(&data_, 0, sizeof(data_));

        encrypted_data_ = std::move(record);
        encryption_algorithm_ = algo;
        encryption_key_version_ = key_version;
        is_encrypted_ = true;
        is_null_ = false;

        return Status::OK;
    }

    Status TypedValue::decrypt(const std::vector<uint8_t>& key, ErrorContext* ctx)
    {
        if (!is_encrypted_) {
            return Status::OK;
        }
        if (encrypted_data_.empty()) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Encrypted payload missing");
            return Status::INVALID_ARGUMENT;
        }

        EncryptedValue encrypted;
        uint32_t key_version = 0;
        Status status = parseEncryptedRecord(encrypted_data_, encrypted, key_version, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<uint8_t> plaintext;
        status = DataEncryption::decrypt(encrypted, key, plaintext, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        status = deserializePlainValue(plaintext, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        encrypted_data_.clear();
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encryption_key_version_ = 0;
        is_encrypted_ = false;
        is_null_ = false;

        return Status::OK;
    }

    Status TypedValue::setEncryptedData(const std::vector<uint8_t>& record, ErrorContext* ctx)
    {
        EncryptedValue encrypted;
        uint32_t key_version = 0;
        Status status = parseEncryptedRecord(record, encrypted, key_version, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        // Clear plaintext payload
        string_data_.clear();
        binary_data_.clear();
        spatial_data_.reset();
        complex_data_.reset();
        std::memset(&data_, 0, sizeof(data_));

        encrypted_data_ = record;
        encryption_algorithm_ = encrypted.algorithm;
        encryption_key_version_ = key_version;
        is_encrypted_ = true;
        is_null_ = false;

        return Status::OK;
    }

    // ===== Setters =====

    void TypedValue::setInt32(int32_t value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
        type_ = DataType::INT32;
        is_null_ = false;
        data_.int32_val = value;
    }

    void TypedValue::setInt64(int64_t value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
        type_ = DataType::INT64;
        is_null_ = false;
        data_.int64_val = value;
    }

    void TypedValue::setFloat32(float value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
        type_ = DataType::FLOAT32;
        is_null_ = false;
        data_.float32_val = value;
    }

    void TypedValue::setFloat64(double value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
        type_ = DataType::FLOAT64;
        is_null_ = false;
        data_.float64_val = value;
    }

    void TypedValue::setBool(bool value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
        type_ = DataType::BOOLEAN;
        is_null_ = false;
        data_.bool_val = value;
    }

    void TypedValue::setVarchar(const std::string& value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
        type_ = DataType::VARCHAR;
        is_null_ = false;
        string_data_ = value;
    }

    void TypedValue::setText(const std::string& value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
        type_ = DataType::TEXT;
        is_null_ = false;
        string_data_ = value;
    }

    void TypedValue::setChar(const std::string& value)
    {
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;
        encrypted_data_.clear();
        type_ = DataType::CHAR;
        is_null_ = false;
        string_data_ = value;
    }

    void TypedValue::setDecimalType(uint8_t precision, uint8_t scale)
    {
        decimal_precision_ = precision;
        decimal_scale_ = scale;
    }

    // ===== Comparison Operators =====

    bool TypedValue::operator==(const TypedValue& other) const
    {
        // NULL handling: NULL == NULL is false in SQL
        if (is_null_ || other.is_null_) {
            return false;
        }
        ensureDecrypted();
        other.ensureDecrypted();

        // Type must match
        if (type_ != other.type_) {
            return false;
        }

        // Compare by type
        switch (type_)
        {
            case DataType::INT8:
                return data_.int8_val == other.data_.int8_val;
            case DataType::INT16:
                return data_.int16_val == other.data_.int16_val;
            case DataType::INT32:
                return data_.int32_val == other.data_.int32_val;
            case DataType::INT64:
                return data_.int64_val == other.data_.int64_val;
            case DataType::INT128:
                return binary_data_ == other.binary_data_;
            case DataType::UINT8:
                return data_.uint8_val == other.data_.uint8_val;
            case DataType::UINT16:
                return data_.uint16_val == other.data_.uint16_val;
            case DataType::UINT32:
                return data_.uint32_val == other.data_.uint32_val;
            case DataType::UINT64:
                return data_.uint64_val == other.data_.uint64_val;
            case DataType::UINT128:
                return binary_data_ == other.binary_data_;
            case DataType::FLOAT32:
                return data_.float32_val == other.data_.float32_val;
            case DataType::FLOAT64:
                return data_.float64_val == other.data_.float64_val;
            case DataType::DECIMAL:
            {
                uint8_t precision = decimal_precision_ == 0
                                        ? (type_ == DataType::DECIMAL ? DECIMAL_MAX_PRECISION
                                                                      : defaultDecfloatPrecision(type_))
                                        : decimal_precision_;
                Decimal left(decimal_unscaled_, precision, decimal_scale_);
                uint8_t other_precision = other.decimal_precision_ == 0
                                              ? DECIMAL_MAX_PRECISION
                                              : other.decimal_precision_;
                Decimal right(other.decimal_unscaled_, other_precision, other.decimal_scale_);
                return left == right;
            }
            case DataType::DECFLOAT16:
            case DataType::DECFLOAT34:
            {
                DecFloat left;
                DecFloat right;
                ErrorContext ctx;
                if (decodeDecfloat(binary_data_, type_, left, &ctx) != Status::OK ||
                    decodeDecfloat(other.binary_data_, other.type_, right, &ctx) != Status::OK)
                {
                    return false;
                }
                return DecFloat::compare(left, right, &ctx) == 0;
            }
            case DataType::BOOLEAN:
                return data_.bool_val == other.data_.bool_val;
            case DataType::VARCHAR:
            case DataType::TEXT:
            case DataType::CHAR:
                return string_data_ == other.string_data_;
            case DataType::DATE:
            case DataType::TIME:
            case DataType::TIMESTAMP:
                return data_.int64_val == other.data_.int64_val;
            case DataType::BINARY:
            case DataType::VARBINARY:
            case DataType::BLOB:
            case DataType::BYTEA:
                return binary_data_ == other.binary_data_;
            case DataType::POINT:
                if (!spatial_data_ || !other.spatial_data_) return false;
                return spatial_data_->point == other.spatial_data_->point;
            case DataType::LINESTRING:
                if (!spatial_data_ || !other.spatial_data_) return false;
                return spatial_data_->linestring == other.spatial_data_->linestring;
            case DataType::POLYGON:
                if (!spatial_data_ || !other.spatial_data_) return false;
                return spatial_data_->polygon == other.spatial_data_->polygon;
            case DataType::MULTIPOINT:
                if (!spatial_data_ || !other.spatial_data_) return false;
                return spatial_data_->multipoint == other.spatial_data_->multipoint;
            case DataType::MULTILINESTRING:
                if (!spatial_data_ || !other.spatial_data_) return false;
                return spatial_data_->multilinestring == other.spatial_data_->multilinestring;
            case DataType::MULTIPOLYGON:
                if (!spatial_data_ || !other.spatial_data_) return false;
                return spatial_data_->multipolygon == other.spatial_data_->multipolygon;
            case DataType::GEOMETRYCOLLECTION:
                if (!spatial_data_ || !other.spatial_data_) return false;
                return spatial_data_->geometrycollection == other.spatial_data_->geometrycollection;
            default:
                // For unimplemented types, consider not equal
                return false;
        }
    }

    bool TypedValue::operator<(const TypedValue& other) const
    {
        // NULL handling: NULL < anything is false
        if (is_null_ || other.is_null_) {
            return false;
        }
        ensureDecrypted();
        other.ensureDecrypted();

        // Type must match
        if (type_ != other.type_) {
            throw std::runtime_error("Cannot compare values of different types");
        }

        // Compare by type
        switch (type_)
        {
            case DataType::INT8:
                return data_.int8_val < other.data_.int8_val;
            case DataType::INT16:
                return data_.int16_val < other.data_.int16_val;
            case DataType::INT32:
                return data_.int32_val < other.data_.int32_val;
            case DataType::INT64:
                return data_.int64_val < other.data_.int64_val;
            case DataType::INT128:
            {
                size_t offset = 0;
                int128_t left_val = 0;
                int128_t right_val = 0;
                std::vector<uint8_t> left_bytes = binary_data_;
                std::vector<uint8_t> right_bytes = other.binary_data_;
                if (!readInt128(left_bytes, offset, 16, left_val) ||
                    !readInt128(right_bytes, offset = 0, 16, right_val))
                {
                    throw std::runtime_error("INT128 decode failed");
                }
                return left_val < right_val;
            }
            case DataType::UINT8:
                return data_.uint8_val < other.data_.uint8_val;
            case DataType::UINT16:
                return data_.uint16_val < other.data_.uint16_val;
            case DataType::UINT32:
                return data_.uint32_val < other.data_.uint32_val;
            case DataType::UINT64:
                return data_.uint64_val < other.data_.uint64_val;
            case DataType::UINT128:
            {
                size_t offset = 0;
                uint128_t left_val = 0;
                uint128_t right_val = 0;
                std::vector<uint8_t> left_bytes = binary_data_;
                std::vector<uint8_t> right_bytes = other.binary_data_;
                if (!readUint128(left_bytes, offset, 16, left_val) ||
                    !readUint128(right_bytes, offset = 0, 16, right_val))
                {
                    throw std::runtime_error("UINT128 decode failed");
                }
                return left_val < right_val;
            }
            case DataType::FLOAT32:
                return data_.float32_val < other.data_.float32_val;
            case DataType::FLOAT64:
                return data_.float64_val < other.data_.float64_val;
            case DataType::DECIMAL:
            {
                uint8_t precision = decimal_precision_ == 0
                                        ? (type_ == DataType::DECIMAL ? DECIMAL_MAX_PRECISION
                                                                      : defaultDecfloatPrecision(type_))
                                        : decimal_precision_;
                Decimal left(decimal_unscaled_, precision, decimal_scale_);
                uint8_t other_precision = other.decimal_precision_ == 0
                                              ? DECIMAL_MAX_PRECISION
                                              : other.decimal_precision_;
                Decimal right(other.decimal_unscaled_, other_precision, other.decimal_scale_);
                return left < right;
            }
            case DataType::DECFLOAT16:
            case DataType::DECFLOAT34:
            {
                DecFloat left;
                DecFloat right;
                ErrorContext ctx;
                if (decodeDecfloat(binary_data_, type_, left, &ctx) != Status::OK ||
                    decodeDecfloat(other.binary_data_, other.type_, right, &ctx) != Status::OK)
                {
                    throw std::runtime_error("DECFLOAT decode failed");
                }
                return DecFloat::compare(left, right, &ctx) < 0;
            }
            case DataType::BOOLEAN:
                return data_.bool_val < other.data_.bool_val;
            case DataType::VARCHAR:
            case DataType::TEXT:
            case DataType::CHAR:
                return string_data_ < other.string_data_;
            case DataType::DATE:
            case DataType::TIME:
            case DataType::TIMESTAMP:
                return data_.int64_val < other.data_.int64_val;
            case DataType::BINARY:
            case DataType::VARBINARY:
            case DataType::BLOB:
            case DataType::BYTEA:
                return binary_data_ < other.binary_data_;
            default:
                throw std::runtime_error("Comparison not supported for this type");
        }
    }

    bool TypedValue::operator<=(const TypedValue& other) const
    {
        return *this == other || *this < other;
    }

    // ===== Helper Methods =====

    void TypedValue::ensureDecrypted() const
    {
        if (is_encrypted_)
        {
            throw std::runtime_error("Value is encrypted");
        }
    }

    Status TypedValue::serializePlainValue(std::vector<uint8_t>& out, ErrorContext* ctx) const
    {
        out.clear();

        if (is_null_) {
            return Status::OK;
        }
        if (is_encrypted_) {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Cannot serialize encrypted value");
            return Status::INVALID_ARGUMENT;
        }

        const auto max_u32 = std::numeric_limits<uint32_t>::max();

        auto appendLengthPrefixedString = [&](const std::string& value) -> Status
        {
            if (value.size() > max_u32)
            {
                SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "String value too large to serialize");
                return Status::OUT_OF_RANGE;
            }
            appendUint32(out, static_cast<uint32_t>(value.size()));
            out.insert(out.end(), value.begin(), value.end());
            return Status::OK;
        };

        auto appendLengthPrefixedBinary = [&](const std::vector<uint8_t>& value) -> Status
        {
            if (value.size() > max_u32)
            {
                SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Binary value too large to serialize");
                return Status::OUT_OF_RANGE;
            }
            appendUint32(out, static_cast<uint32_t>(value.size()));
            out.insert(out.end(), value.begin(), value.end());
            return Status::OK;
        };

        auto serializeValueList = [&](const std::vector<TypedValue>& values) -> Status
        {
            if (values.size() > max_u32)
            {
                SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Too many elements to serialize");
                return Status::OUT_OF_RANGE;
            }
            appendUint32(out, static_cast<uint32_t>(values.size()));
            for (const auto& element : values)
            {
                appendUint8(out, element.isNull() ? 1 : 0);
                appendUint16(out, static_cast<uint16_t>(element.type()));

                std::vector<uint8_t> element_data;
                if (!element.isNull())
                {
                    Status status = element.serializePlainValue(element_data, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }
                }

                if (element_data.size() > max_u32)
                {
                    SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Element too large to serialize");
                    return Status::OUT_OF_RANGE;
                }
                appendUint32(out, static_cast<uint32_t>(element_data.size()));
                out.insert(out.end(), element_data.begin(), element_data.end());
            }
            return Status::OK;
        };

        switch (type_)
        {
            case DataType::INT8:
                appendUint8(out, static_cast<uint8_t>(data_.int8_val));
                break;
            case DataType::INT16:
                appendUint16(out, static_cast<uint16_t>(data_.int16_val));
                break;
            case DataType::INT32:
                appendInt32(out, data_.int32_val);
                break;
            case DataType::INT64:
                appendInt64(out, data_.int64_val);
                break;
            case DataType::UINT8:
                appendUint8(out, data_.uint8_val);
                break;
            case DataType::UINT16:
                appendUint16(out, data_.uint16_val);
                break;
            case DataType::UINT32:
                appendUint32(out, data_.uint32_val);
                break;
            case DataType::UINT64:
                appendUint64(out, data_.uint64_val);
                break;
            case DataType::FLOAT32:
                appendFloat(out, data_.float32_val);
                break;
            case DataType::FLOAT64:
                appendDouble(out, data_.float64_val);
                break;
            case DataType::BOOLEAN:
                appendUint8(out, data_.bool_val ? 1 : 0);
                break;
            case DataType::MONEY:
                appendInt64(out, data_.int64_val);
                break;
            case DataType::CHAR:
            case DataType::VARCHAR:
            case DataType::TEXT:
            case DataType::JSON:
            case DataType::XML:
            {
                Status status = appendLengthPrefixedString(string_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::JSONB:
            {
                std::vector<uint8_t> payload = binary_data_;
                if (payload.empty() && !string_data_.empty())
                {
                    if (!encodeJsonb(string_data_, payload, ctx))
                    {
                        return Status::INVALID_TEXT_REPRESENTATION;
                    }
                }
                Status status = appendLengthPrefixedBinary(payload);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::DECIMAL:
            {
                uint8_t precision = decimal_precision_ == 0
                                        ? (type_ == DataType::DECIMAL ? DECIMAL_MAX_PRECISION
                                                                      : defaultDecfloatPrecision(type_))
                                        : decimal_precision_;
                if (precision > DECIMAL_MAX_PRECISION || decimal_scale_ > precision)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Invalid DECIMAL precision/scale");
                    return Status::INVALID_ARGUMENT;
                }

                size_t width = decimalStorageSize(precision);
                if (width == 0)
                {
                    SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "DECIMAL precision not supported");
                    return Status::OUT_OF_RANGE;
                }

                int128_t max_abs = POWERS_OF_10[precision] - 1;
                if (decimal_unscaled_ > max_abs || decimal_unscaled_ < -max_abs)
                {
                    SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                      "DECIMAL value out of range");
                    return Status::NUMERIC_VALUE_OUT_OF_RANGE;
                }

                appendInt128(out, decimal_unscaled_, width);
                break;
            }
            case DataType::DECFLOAT16:
            case DataType::DECFLOAT34:
            {
                std::vector<uint8_t> bytes;
                DecFloat df;
                Status st = decodeDecfloat(binary_data_, type_, df, ctx);
                if (st != Status::OK)
                {
                    return st;
                }
                st = encodeDecfloat(df, type_, bytes, ctx);
                if (st != Status::OK)
                {
                    return st;
                }
                out.insert(out.end(), bytes.begin(), bytes.end());
                break;
            }
            case DataType::BINARY:
            case DataType::VARBINARY:
            case DataType::BLOB:
            case DataType::BYTEA:
            {
                Status status = appendLengthPrefixedBinary(binary_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::DATE:
            {
                int64_t days = data_.int64_val;
                int64_t mjd64 = days + FirebirdDateTime::UNIX_EPOCH_MJD;
                if (mjd64 < std::numeric_limits<int32_t>::min() ||
                    mjd64 > std::numeric_limits<int32_t>::max())
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATETIME_FIELD_OVERFLOW, "DATE out of range");
                    return Status::DATETIME_FIELD_OVERFLOW;
                }
                appendInt32(out, static_cast<int32_t>(mjd64));
                appendInt32(out, timezone_offset_seconds_);
                break;
            }
            case DataType::TIME:
            {
                int64_t micros = data_.int64_val;
                appendInt64(out, micros);
                appendInt32(out, timezone_offset_seconds_);
                break;
            }
            case DataType::TIMESTAMP:
            {
                int64_t micros = data_.int64_val;
                appendInt64(out, micros);
                appendInt32(out, timezone_offset_seconds_);
                break;
            }
            case DataType::UUID:
            case DataType::INT128:
            case DataType::UINT128:
            {
                if (binary_data_.size() != 16)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Expected 16 bytes for UUID/INT128/UINT128");
                    return Status::INVALID_ARGUMENT;
                }
                out.insert(out.end(), binary_data_.begin(), binary_data_.end());
                break;
            }
            case DataType::VECTOR:
            {
                Status status = appendLengthPrefixedBinary(binary_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::POINT:
            {
                if (!spatial_data_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Spatial data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                appendPoint(out, spatial_data_->point);
                break;
            }
            case DataType::LINESTRING:
            {
                if (!spatial_data_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Spatial data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& line = spatial_data_->linestring;
                appendInt32(out, line.srid);
                appendPointList(out, line.points);
                break;
            }
            case DataType::POLYGON:
            {
                if (!spatial_data_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Spatial data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& polygon = spatial_data_->polygon;
                appendInt32(out, polygon.srid);
                appendUint32(out, static_cast<uint32_t>(polygon.rings.size()));
                for (const auto& ring : polygon.rings)
                {
                    appendPointList(out, ring);
                }
                break;
            }
            case DataType::MULTIPOINT:
            {
                if (!spatial_data_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Spatial data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& multi = spatial_data_->multipoint;
                appendInt32(out, multi.srid);
                appendPointList(out, multi.points);
                break;
            }
            case DataType::MULTILINESTRING:
            {
                if (!spatial_data_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Spatial data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& multi = spatial_data_->multilinestring;
                appendInt32(out, multi.srid);
                appendUint32(out, static_cast<uint32_t>(multi.linestrings.size()));
                for (const auto& line : multi.linestrings)
                {
                    appendInt32(out, line.srid);
                    appendPointList(out, line.points);
                }
                break;
            }
            case DataType::MULTIPOLYGON:
            {
                if (!spatial_data_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Spatial data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& multi = spatial_data_->multipolygon;
                appendInt32(out, multi.srid);
                appendUint32(out, static_cast<uint32_t>(multi.polygons.size()));
                for (const auto& polygon : multi.polygons)
                {
                    appendInt32(out, polygon.srid);
                    appendUint32(out, static_cast<uint32_t>(polygon.rings.size()));
                    for (const auto& ring : polygon.rings)
                    {
                        appendPointList(out, ring);
                    }
                }
                break;
            }
            case DataType::GEOMETRYCOLLECTION:
            {
                if (!spatial_data_)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Spatial data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& collection = spatial_data_->geometrycollection;
                appendInt32(out, collection.srid);
                if (collection.geometries.size() > max_u32)
                {
                    SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Geometry collection too large to serialize");
                    return Status::OUT_OF_RANGE;
                }
                appendUint32(out, static_cast<uint32_t>(collection.geometries.size()));
                for (const auto& geom : collection.geometries)
                {
                    appendUint8(out, geom ? 0 : 1);
                    if (!geom)
                    {
                        appendUint16(out, 0);
                        appendUint32(out, 0);
                        continue;
                    }
                    appendUint16(out, static_cast<uint16_t>(geom->type()));
                    std::vector<uint8_t> geom_data;
                    Status status = geom->serializePlainValue(geom_data, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }
                    if (geom_data.size() > max_u32)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::OUT_OF_RANGE, "Geometry too large to serialize");
                        return Status::OUT_OF_RANGE;
                    }
                    appendUint32(out, static_cast<uint32_t>(geom_data.size()));
                    out.insert(out.end(), geom_data.begin(), geom_data.end());
                }
                break;
            }
            case DataType::INET:
            {
                if (!complex_data_ || !complex_data_->inet)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "INET data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& inet = *complex_data_->inet;
                appendUint8(out, static_cast<uint8_t>(inet.family()));
                appendUint8(out, inet.netmask());
                out.insert(out.end(), inet.data(), inet.data() + inet.size());
                break;
            }
            case DataType::CIDR:
            {
                if (!complex_data_ || !complex_data_->cidr)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CIDR data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto inet = complex_data_->cidr->toInet();
                appendUint8(out, static_cast<uint8_t>(inet.family()));
                appendUint8(out, inet.netmask());
                out.insert(out.end(), inet.data(), inet.data() + inet.size());
                break;
            }
            case DataType::MACADDR:
            {
                if (!complex_data_ || !complex_data_->macaddr)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "MACADDR data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& bytes = complex_data_->macaddr->bytes();
                out.insert(out.end(), bytes.begin(), bytes.end());
                break;
            }
            case DataType::MACADDR8:
            {
                if (!complex_data_ || !complex_data_->macaddr8)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "MACADDR8 data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& bytes = complex_data_->macaddr8->bytes();
                out.insert(out.end(), bytes.begin(), bytes.end());
                break;
            }
            case DataType::INTERVAL:
            {
                if (!complex_data_ || !complex_data_->interval)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Interval data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                const auto& interval = *complex_data_->interval;
                appendInt32(out, interval.months);
                appendInt32(out, interval.days);
                appendInt64(out, interval.microseconds);
                break;
            }
            case DataType::TSVECTOR:
            {
                if (!complex_data_ || !complex_data_->tsvector)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "TSVector data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                out = complex_data_->tsvector->toBinary();
                break;
            }
            case DataType::TSQUERY:
            {
                if (!complex_data_ || !complex_data_->tsquery)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "TSQuery data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                out = complex_data_->tsquery->toBinary();
                break;
            }
            case DataType::INT4RANGE:
            {
                if (!complex_data_ || !complex_data_->range_data)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Range data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                auto range = std::static_pointer_cast<Range<int32_t>>(complex_data_->range_data);
                uint8_t flags = 0;
                if (range->isEmpty())
                {
                    appendUint8(out, 0x01);
                    break;
                }
                if (range->isLowerBounded()) flags |= 0x02;
                if (range->isUpperBounded()) flags |= 0x04;
                if (range->lowerBoundType() == BoundType::INCLUSIVE) flags |= 0x08;
                if (range->upperBoundType() == BoundType::INCLUSIVE) flags |= 0x10;
                appendUint8(out, flags);
                if (flags & 0x02) appendInt32(out, *range->lower());
                if (flags & 0x04) appendInt32(out, *range->upper());
                break;
            }
            case DataType::INT8RANGE:
            case DataType::DATERANGE:
            case DataType::TSRANGE:
            case DataType::TSTZRANGE:
            {
                if (!complex_data_ || !complex_data_->range_data)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Range data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                auto range = std::static_pointer_cast<Range<int64_t>>(complex_data_->range_data);
                uint8_t flags = 0;
                if (range->isEmpty())
                {
                    appendUint8(out, 0x01);
                    break;
                }
                if (range->isLowerBounded()) flags |= 0x02;
                if (range->isUpperBounded()) flags |= 0x04;
                if (range->lowerBoundType() == BoundType::INCLUSIVE) flags |= 0x08;
                if (range->upperBoundType() == BoundType::INCLUSIVE) flags |= 0x10;
                appendUint8(out, flags);
                if (flags & 0x02) appendInt64(out, *range->lower());
                if (flags & 0x04) appendInt64(out, *range->upper());
                break;
            }
            case DataType::NUMRANGE:
            {
                if (!complex_data_ || !complex_data_->range_data)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Range data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                auto range = std::static_pointer_cast<Range<double>>(complex_data_->range_data);
                uint8_t flags = 0;
                if (range->isEmpty())
                {
                    appendUint8(out, 0x01);
                    break;
                }
                if (range->isLowerBounded()) flags |= 0x02;
                if (range->isUpperBounded()) flags |= 0x04;
                if (range->lowerBoundType() == BoundType::INCLUSIVE) flags |= 0x08;
                if (range->upperBoundType() == BoundType::INCLUSIVE) flags |= 0x10;
                appendUint8(out, flags);
                if (flags & 0x02) appendDouble(out, *range->lower());
                if (flags & 0x04) appendDouble(out, *range->upper());
                break;
            }
            case DataType::ARRAY:
            {
                if (!complex_data_ || !complex_data_->array)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Array data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                Status status = serializeValueList(*complex_data_->array);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::COMPOSITE:
            {
                if (!complex_data_ || !complex_data_->array)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Composite data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                Status status = appendLengthPrefixedString(string_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                status = serializeValueList(*complex_data_->array);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::VARIANT:
            {
                if (!complex_data_ || !complex_data_->array)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Variant data not initialized");
                    return Status::INVALID_ARGUMENT;
                }
                Status status = serializeValueList(*complex_data_->array);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            default:
                SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Unsupported type for encryption serialization");
                return Status::NOT_SUPPORTED;
        }

        return Status::OK;
    }

    Status TypedValue::deserializePlainValue(const std::vector<uint8_t>& data, ErrorContext* ctx)
    {
        if (type_ == DataType::NULL_TYPE)
        {
            is_null_ = true;
            return Status::OK;
        }

        if (data.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Empty plaintext for non-null value");
            return Status::DATA_CORRUPTED;
        }

        string_data_.clear();
        binary_data_.clear();
        spatial_data_.reset();
        complex_data_.reset();
        std::memset(&data_, 0, sizeof(data_));

        size_t offset = 0;

        auto readLengthPrefixedString = [&](std::string& value_out) -> Status
        {
            uint32_t len = 0;
            if (!readUint32(data, offset, len))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid length prefix for string");
                return Status::DATA_CORRUPTED;
            }
            if (offset + len > data.size())
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "String length exceeds payload");
                return Status::DATA_CORRUPTED;
            }
            value_out.assign(reinterpret_cast<const char*>(data.data() + offset), len);
            offset += len;
            return Status::OK;
        };

        auto readLengthPrefixedBinary = [&](std::vector<uint8_t>& value_out) -> Status
        {
            uint32_t len = 0;
            if (!readUint32(data, offset, len))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid length prefix for binary");
                return Status::DATA_CORRUPTED;
            }
            if (!readBytes(data, offset, len, value_out))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Binary length exceeds payload");
                return Status::DATA_CORRUPTED;
            }
            return Status::OK;
        };

        auto deserializeValueList = [&](std::vector<TypedValue>& values_out) -> Status
        {
            uint32_t count = 0;
            if (!readUint32(data, offset, count))
            {
                SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid element count");
                return Status::DATA_CORRUPTED;
            }
            values_out.clear();
            values_out.reserve(count);

            for (uint32_t i = 0; i < count; ++i)
            {
                uint8_t is_null = 0;
                uint16_t type_code = 0;
                uint32_t len = 0;
                if (!readUint8(data, offset, is_null) ||
                    !readUint16(data, offset, type_code) ||
                    !readUint32(data, offset, len))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid element header");
                    return Status::DATA_CORRUPTED;
                }

                TypedValue element(static_cast<DataType>(type_code));

                if (is_null)
                {
                    if (len != 0)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Null element has payload");
                        return Status::DATA_CORRUPTED;
                    }
                    values_out.push_back(std::move(element));
                    continue;
                }

                std::vector<uint8_t> element_data;
                if (!readBytes(data, offset, len, element_data))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Element payload exceeds buffer");
                    return Status::DATA_CORRUPTED;
                }

                Status status = element.deserializePlainValue(element_data, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
                values_out.push_back(std::move(element));
            }

            return Status::OK;
        };

        Status status = Status::OK;

        switch (type_)
        {
            case DataType::INT8:
            {
                uint8_t value = 0;
                if (!readUint8(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT8 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.int8_val = static_cast<int8_t>(value);
                break;
            }
            case DataType::INT16:
            {
                uint16_t value = 0;
                if (!readUint16(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT16 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.int16_val = static_cast<int16_t>(value);
                break;
            }
            case DataType::INT32:
            {
                int32_t value = 0;
                if (!readInt32(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT32 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.int32_val = value;
                break;
            }
            case DataType::INT64:
            {
                int64_t value = 0;
                if (!readInt64(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT64 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.int64_val = value;
                break;
            }
            case DataType::UINT8:
            {
                uint8_t value = 0;
                if (!readUint8(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid UINT8 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.uint8_val = value;
                break;
            }
            case DataType::UINT16:
            {
                uint16_t value = 0;
                if (!readUint16(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid UINT16 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.uint16_val = value;
                break;
            }
            case DataType::UINT32:
            {
                uint32_t value = 0;
                if (!readUint32(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid UINT32 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.uint32_val = value;
                break;
            }
            case DataType::UINT64:
            {
                uint64_t value = 0;
                if (!readUint64(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid UINT64 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.uint64_val = value;
                break;
            }
            case DataType::FLOAT32:
            {
                float value = 0.0f;
                if (!readFloat(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid FLOAT32 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.float32_val = value;
                break;
            }
            case DataType::FLOAT64:
            {
                double value = 0.0;
                if (!readDouble(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid FLOAT64 payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.float64_val = value;
                break;
            }
            case DataType::BOOLEAN:
            {
                uint8_t value = 0;
                if (!readUint8(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid BOOLEAN payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.bool_val = (value != 0);
                break;
            }
            case DataType::MONEY:
            {
                int64_t value = 0;
                if (!readInt64(data, offset, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MONEY payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.int64_val = value;
                break;
            }
            case DataType::CHAR:
            case DataType::VARCHAR:
            case DataType::TEXT:
            case DataType::JSON:
            case DataType::XML:
            {
                status = readLengthPrefixedString(string_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::JSONB:
            {
                status = readLengthPrefixedBinary(binary_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::DECIMAL:
            {
                uint8_t precision = decimal_precision_ == 0
                                        ? (type_ == DataType::DECIMAL ? DECIMAL_MAX_PRECISION
                                                                      : defaultDecfloatPrecision(type_))
                                        : decimal_precision_;
                if (precision > DECIMAL_MAX_PRECISION || decimal_scale_ > precision)
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid DECIMAL precision/scale");
                    return Status::DATA_CORRUPTED;
                }

                size_t width = decimalStorageSize(precision);
                int128_t value = 0;
                if (width == 0 || !readInt128(data, offset, width, value))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid DECIMAL payload");
                    return Status::DATA_CORRUPTED;
                }
                decimal_unscaled_ = value;
                decimal_precision_ = precision;
                break;
            }
            case DataType::DECFLOAT16:
            case DataType::DECFLOAT34:
            {
                size_t expected = type_ == DataType::DECFLOAT16 ? 8 : 16;
                if (offset + expected > data.size())
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid DECFLOAT payload");
                    return Status::DATA_CORRUPTED;
                }
                binary_data_.assign(data.begin() + offset, data.begin() + offset + expected);
                offset += expected;
                break;
            }
            case DataType::DATE:
            {
                int32_t mjd = 0;
                int32_t offset_seconds = 0;
                if (!readInt32(data, offset, mjd) ||
                    !readInt32(data, offset, offset_seconds))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid DATE payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.int64_val = static_cast<int64_t>(mjd) - FirebirdDateTime::UNIX_EPOCH_MJD;
                timezone_offset_seconds_ = offset_seconds;
                break;
            }
            case DataType::TIME:
            {
                int64_t micros = 0;
                int32_t offset_seconds = 0;
                if (!readInt64(data, offset, micros) ||
                    !readInt32(data, offset, offset_seconds))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid TIME payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.int64_val = micros;
                timezone_offset_seconds_ = offset_seconds;
                break;
            }
            case DataType::TIMESTAMP:
            {
                int64_t micros = 0;
                int32_t offset_seconds = 0;
                if (!readInt64(data, offset, micros) ||
                    !readInt32(data, offset, offset_seconds))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid TIMESTAMP payload");
                    return Status::DATA_CORRUPTED;
                }
                data_.int64_val = micros;
                timezone_offset_seconds_ = offset_seconds;
                break;
            }
            case DataType::BINARY:
            case DataType::VARBINARY:
            case DataType::BLOB:
            case DataType::BYTEA:
            {
                status = readLengthPrefixedBinary(binary_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::UUID:
            case DataType::INT128:
            case DataType::UINT128:
            {
                if (data.size() != 16)
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED,
                                      "Invalid UUID/INT128/UINT128 payload size");
                    return Status::DATA_CORRUPTED;
                }
                binary_data_.assign(data.begin(), data.end());
                offset = data.size();
                break;
            }
            case DataType::VECTOR:
            {
                status = readLengthPrefixedBinary(binary_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                if (binary_data_.size() % sizeof(float) != 0)
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Vector payload length is not float-aligned");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::POINT:
            {
                spatial_data_ = std::make_unique<SpatialData>();
                if (!readPoint(data, offset, spatial_data_->point))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid POINT payload");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::LINESTRING:
            {
                spatial_data_ = std::make_unique<SpatialData>();
                int32_t srid = 0;
                if (!readInt32(data, offset, srid))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid LINESTRING payload");
                    return Status::DATA_CORRUPTED;
                }
                spatial_data_->linestring.srid = srid;
                if (!readPointList(data, offset, spatial_data_->linestring.points))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid LINESTRING points");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::POLYGON:
            {
                spatial_data_ = std::make_unique<SpatialData>();
                int32_t srid = 0;
                uint32_t ring_count = 0;
                if (!readInt32(data, offset, srid) || !readUint32(data, offset, ring_count))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid POLYGON header");
                    return Status::DATA_CORRUPTED;
                }
                spatial_data_->polygon.srid = srid;
                spatial_data_->polygon.rings.clear();
                spatial_data_->polygon.rings.reserve(ring_count);
                for (uint32_t i = 0; i < ring_count; ++i)
                {
                    std::vector<Point> ring;
                    if (!readPointList(data, offset, ring))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid POLYGON ring");
                        return Status::DATA_CORRUPTED;
                    }
                    spatial_data_->polygon.rings.push_back(std::move(ring));
                }
                break;
            }
            case DataType::MULTIPOINT:
            {
                spatial_data_ = std::make_unique<SpatialData>();
                int32_t srid = 0;
                if (!readInt32(data, offset, srid))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTIPOINT header");
                    return Status::DATA_CORRUPTED;
                }
                spatial_data_->multipoint.srid = srid;
                if (!readPointList(data, offset, spatial_data_->multipoint.points))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTIPOINT points");
                    return Status::DATA_CORRUPTED;
                }
                break;
            }
            case DataType::MULTILINESTRING:
            {
                spatial_data_ = std::make_unique<SpatialData>();
                int32_t srid = 0;
                uint32_t line_count = 0;
                if (!readInt32(data, offset, srid) || !readUint32(data, offset, line_count))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTILINESTRING header");
                    return Status::DATA_CORRUPTED;
                }
                spatial_data_->multilinestring.srid = srid;
                spatial_data_->multilinestring.linestrings.clear();
                spatial_data_->multilinestring.linestrings.reserve(line_count);
                for (uint32_t i = 0; i < line_count; ++i)
                {
                    LineString line;
                    if (!readInt32(data, offset, line.srid) ||
                        !readPointList(data, offset, line.points))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTILINESTRING segment");
                        return Status::DATA_CORRUPTED;
                    }
                    spatial_data_->multilinestring.linestrings.push_back(std::move(line));
                }
                break;
            }
            case DataType::MULTIPOLYGON:
            {
                spatial_data_ = std::make_unique<SpatialData>();
                int32_t srid = 0;
                uint32_t polygon_count = 0;
                if (!readInt32(data, offset, srid) || !readUint32(data, offset, polygon_count))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTIPOLYGON header");
                    return Status::DATA_CORRUPTED;
                }
                spatial_data_->multipolygon.srid = srid;
                spatial_data_->multipolygon.polygons.clear();
                spatial_data_->multipolygon.polygons.reserve(polygon_count);
                for (uint32_t i = 0; i < polygon_count; ++i)
                {
                    Polygon polygon;
                    uint32_t ring_count = 0;
                    if (!readInt32(data, offset, polygon.srid) ||
                        !readUint32(data, offset, ring_count))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTIPOLYGON polygon header");
                        return Status::DATA_CORRUPTED;
                    }
                    polygon.rings.clear();
                    polygon.rings.reserve(ring_count);
                    for (uint32_t r = 0; r < ring_count; ++r)
                    {
                        std::vector<Point> ring;
                        if (!readPointList(data, offset, ring))
                        {
                            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MULTIPOLYGON ring");
                            return Status::DATA_CORRUPTED;
                        }
                        polygon.rings.push_back(std::move(ring));
                    }
                    spatial_data_->multipolygon.polygons.push_back(std::move(polygon));
                }
                break;
            }
            case DataType::GEOMETRYCOLLECTION:
            {
                spatial_data_ = std::make_unique<SpatialData>();
                int32_t srid = 0;
                uint32_t geom_count = 0;
                if (!readInt32(data, offset, srid) || !readUint32(data, offset, geom_count))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid GEOMETRYCOLLECTION header");
                    return Status::DATA_CORRUPTED;
                }
                spatial_data_->geometrycollection.srid = srid;
                spatial_data_->geometrycollection.geometries.clear();
                spatial_data_->geometrycollection.geometries.reserve(geom_count);
                for (uint32_t i = 0; i < geom_count; ++i)
                {
                    uint8_t is_null = 0;
                    uint16_t type_code = 0;
                    uint32_t len = 0;
                    if (!readUint8(data, offset, is_null) ||
                        !readUint16(data, offset, type_code) ||
                        !readUint32(data, offset, len))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid GEOMETRYCOLLECTION entry");
                        return Status::DATA_CORRUPTED;
                    }
                    if (is_null)
                    {
                        if (len != 0)
                        {
                            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Null geometry has payload");
                            return Status::DATA_CORRUPTED;
                        }
                        spatial_data_->geometrycollection.geometries.push_back(nullptr);
                        continue;
                    }
                    std::vector<uint8_t> geom_data;
                    if (!readBytes(data, offset, len, geom_data))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Geometry payload exceeds buffer");
                        return Status::DATA_CORRUPTED;
                    }
                    auto geom = std::make_shared<TypedValue>(static_cast<DataType>(type_code));
                    status = geom->deserializePlainValue(geom_data, ctx);
                    if (status != Status::OK)
                    {
                        return status;
                    }
                    spatial_data_->geometrycollection.geometries.push_back(std::move(geom));
                }
                break;
            }
            case DataType::INET:
            {
                uint8_t family = 0;
                uint8_t netmask = 0;
                if (!readUint8(data, offset, family) || !readUint8(data, offset, netmask))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INET header");
                    return Status::DATA_CORRUPTED;
                }
                if (family != static_cast<uint8_t>(AddressFamily::IPv4) &&
                    family != static_cast<uint8_t>(AddressFamily::IPv6))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INET address family");
                    return Status::DATA_CORRUPTED;
                }
                size_t addr_len = (family == static_cast<uint8_t>(AddressFamily::IPv4)) ? 4 : 16;
                std::vector<uint8_t> addr_bytes;
                if (!readBytes(data, offset, addr_len, addr_bytes))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INET address bytes");
                    return Status::DATA_CORRUPTED;
                }
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->inet = std::make_unique<InetAddr>(
                    static_cast<AddressFamily>(family), addr_bytes.data(), netmask);
                break;
            }
            case DataType::CIDR:
            {
                uint8_t family = 0;
                uint8_t netmask = 0;
                if (!readUint8(data, offset, family) || !readUint8(data, offset, netmask))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid CIDR header");
                    return Status::DATA_CORRUPTED;
                }
                if (family != static_cast<uint8_t>(AddressFamily::IPv4) &&
                    family != static_cast<uint8_t>(AddressFamily::IPv6))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid CIDR address family");
                    return Status::DATA_CORRUPTED;
                }
                size_t addr_len = (family == static_cast<uint8_t>(AddressFamily::IPv4)) ? 4 : 16;
                std::vector<uint8_t> addr_bytes;
                if (!readBytes(data, offset, addr_len, addr_bytes))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid CIDR address bytes");
                    return Status::DATA_CORRUPTED;
                }
                complex_data_ = std::make_unique<ComplexData>();
                InetAddr addr(static_cast<AddressFamily>(family), addr_bytes.data(), netmask);
                complex_data_->cidr = std::make_unique<Cidr>(addr);
                break;
            }
            case DataType::MACADDR:
            {
                std::vector<uint8_t> bytes;
                if (!readBytes(data, offset, 6, bytes))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MACADDR bytes");
                    return Status::DATA_CORRUPTED;
                }
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->macaddr = std::make_unique<MacAddr>(MacAddr::fromBytes(bytes.data()));
                break;
            }
            case DataType::MACADDR8:
            {
                std::vector<uint8_t> bytes;
                if (!readBytes(data, offset, 8, bytes))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid MACADDR8 bytes");
                    return Status::DATA_CORRUPTED;
                }
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->macaddr8 = std::make_unique<MacAddr8>(MacAddr8::fromBytes(bytes.data()));
                break;
            }
            case DataType::INTERVAL:
            {
                int32_t months = 0;
                int32_t days = 0;
                int64_t micros = 0;
                if (!readInt32(data, offset, months) ||
                    !readInt32(data, offset, days) ||
                    !readInt64(data, offset, micros))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INTERVAL payload");
                    return Status::DATA_CORRUPTED;
                }
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->interval = std::make_unique<Interval>(months, days, micros);
                break;
            }
            case DataType::TSVECTOR:
            {
                auto parsed = TSVector::fromBinary(data);
                if (!parsed)
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid TSVECTOR payload");
                    return Status::DATA_CORRUPTED;
                }
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->tsvector = std::make_shared<TSVector>(*parsed);
                offset = data.size();
                break;
            }
            case DataType::TSQUERY:
            {
                auto parsed = TSQuery::fromBinary(data);
                if (!parsed)
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid TSQUERY payload");
                    return Status::DATA_CORRUPTED;
                }
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->tsquery = std::make_shared<TSQuery>(std::move(*parsed));
                offset = data.size();
                break;
            }
            case DataType::INT4RANGE:
            {
                uint8_t flags = 0;
                if (!readUint8(data, offset, flags))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT4RANGE flags");
                    return Status::DATA_CORRUPTED;
                }
                if (flags & 0x01)
                {
                    complex_data_ = std::make_unique<ComplexData>();
                    complex_data_->range_data = std::make_shared<Range<int32_t>>();
                    break;
                }
                std::optional<int32_t> lower;
                std::optional<int32_t> upper;
                if (flags & 0x02)
                {
                    int32_t val = 0;
                    if (!readInt32(data, offset, val))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT4RANGE lower bound");
                        return Status::DATA_CORRUPTED;
                    }
                    lower = val;
                }
                if (flags & 0x04)
                {
                    int32_t val = 0;
                    if (!readInt32(data, offset, val))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid INT4RANGE upper bound");
                        return Status::DATA_CORRUPTED;
                    }
                    upper = val;
                }
                BoundType lower_type = (flags & 0x08) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                BoundType upper_type = (flags & 0x10) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->range_data = std::make_shared<Range<int32_t>>(lower, upper, lower_type, upper_type);
                break;
            }
            case DataType::INT8RANGE:
            case DataType::DATERANGE:
            case DataType::TSRANGE:
            case DataType::TSTZRANGE:
            {
                uint8_t flags = 0;
                if (!readUint8(data, offset, flags))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid range flags");
                    return Status::DATA_CORRUPTED;
                }
                if (flags & 0x01)
                {
                    complex_data_ = std::make_unique<ComplexData>();
                    complex_data_->range_data = std::make_shared<Range<int64_t>>();
                    break;
                }
                std::optional<int64_t> lower;
                std::optional<int64_t> upper;
                if (flags & 0x02)
                {
                    int64_t val = 0;
                    if (!readInt64(data, offset, val))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid range lower bound");
                        return Status::DATA_CORRUPTED;
                    }
                    lower = val;
                }
                if (flags & 0x04)
                {
                    int64_t val = 0;
                    if (!readInt64(data, offset, val))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid range upper bound");
                        return Status::DATA_CORRUPTED;
                    }
                    upper = val;
                }
                BoundType lower_type = (flags & 0x08) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                BoundType upper_type = (flags & 0x10) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->range_data = std::make_shared<Range<int64_t>>(lower, upper, lower_type, upper_type);
                break;
            }
            case DataType::NUMRANGE:
            {
                uint8_t flags = 0;
                if (!readUint8(data, offset, flags))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid NUMRANGE flags");
                    return Status::DATA_CORRUPTED;
                }
                if (flags & 0x01)
                {
                    complex_data_ = std::make_unique<ComplexData>();
                    complex_data_->range_data = std::make_shared<Range<double>>();
                    break;
                }
                std::optional<double> lower;
                std::optional<double> upper;
                if (flags & 0x02)
                {
                    double val = 0.0;
                    if (!readDouble(data, offset, val))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid NUMRANGE lower bound");
                        return Status::DATA_CORRUPTED;
                    }
                    lower = val;
                }
                if (flags & 0x04)
                {
                    double val = 0.0;
                    if (!readDouble(data, offset, val))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Invalid NUMRANGE upper bound");
                        return Status::DATA_CORRUPTED;
                    }
                    upper = val;
                }
                BoundType lower_type = (flags & 0x08) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                BoundType upper_type = (flags & 0x10) ? BoundType::INCLUSIVE : BoundType::EXCLUSIVE;
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->range_data = std::make_shared<Range<double>>(lower, upper, lower_type, upper_type);
                break;
            }
            case DataType::ARRAY:
            {
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->array = std::make_unique<std::vector<TypedValue>>();
                status = deserializeValueList(*complex_data_->array);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::COMPOSITE:
            {
                status = readLengthPrefixedString(string_data_);
                if (status != Status::OK)
                {
                    return status;
                }
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->array = std::make_unique<std::vector<TypedValue>>();
                status = deserializeValueList(*complex_data_->array);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            case DataType::VARIANT:
            {
                complex_data_ = std::make_unique<ComplexData>();
                complex_data_->array = std::make_unique<std::vector<TypedValue>>();
                status = deserializeValueList(*complex_data_->array);
                if (status != Status::OK)
                {
                    return status;
                }
                break;
            }
            default:
                SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED, "Unsupported type for encryption deserialization");
                return Status::NOT_SUPPORTED;
        }

        if (offset != data.size())
        {
            SET_ERROR_CONTEXT(ctx, Status::DATA_CORRUPTED, "Trailing bytes in plaintext payload");
            return Status::DATA_CORRUPTED;
        }

        is_null_ = false;
        return Status::OK;
    }

    void TypedValue::copyFrom(const TypedValue& other)
    {
        is_encrypted_ = other.is_encrypted_;
        encryption_key_version_ = other.encryption_key_version_;
        encryption_algorithm_ = other.encryption_algorithm_;
        encrypted_data_ = other.encrypted_data_;
        decimal_unscaled_ = other.decimal_unscaled_;
        decimal_precision_ = other.decimal_precision_;
        decimal_scale_ = other.decimal_scale_;
        timezone_offset_seconds_ = other.timezone_offset_seconds_;

        if (other.is_null_) {
            return;  // Nothing to copy for NULL values
        }

        // Copy primitive data
        std::memcpy(&data_, &other.data_, sizeof(data_));

        // Copy string data
        string_data_ = other.string_data_;

        // Copy binary data
        binary_data_ = other.binary_data_;

        // Copy spatial data
        if (other.spatial_data_) {
            spatial_data_ = std::make_unique<SpatialData>(*other.spatial_data_);
        }

        // Copy complex data
        if (other.complex_data_) {
            complex_data_ = std::make_unique<ComplexData>();
            if (other.complex_data_->inet) {
                complex_data_->inet = std::make_unique<InetAddr>(*other.complex_data_->inet);
            }
            if (other.complex_data_->cidr) {
                complex_data_->cidr = std::make_unique<Cidr>(*other.complex_data_->cidr);
            }
            if (other.complex_data_->macaddr) {
                complex_data_->macaddr = std::make_unique<MacAddr>(*other.complex_data_->macaddr);
            }
            if (other.complex_data_->macaddr8) {
                complex_data_->macaddr8 = std::make_unique<MacAddr8>(*other.complex_data_->macaddr8);
            }
            if (other.complex_data_->tsvector) {
                complex_data_->tsvector = other.complex_data_->tsvector; // shared_ptr copy
            }
            if (other.complex_data_->tsquery) {
                complex_data_->tsquery = other.complex_data_->tsquery; // shared_ptr copy
            }
            if (other.complex_data_->interval) {
                complex_data_->interval = std::make_unique<Interval>(*other.complex_data_->interval);
            }
            if (other.complex_data_->array) {
                complex_data_->array = std::make_unique<std::vector<TypedValue>>(*other.complex_data_->array);
            }
            complex_data_->range_data = other.complex_data_->range_data; // shared_ptr copy
        }
    }

    void TypedValue::moveFrom(TypedValue&& other) noexcept
    {
        is_encrypted_ = other.is_encrypted_;
        encryption_key_version_ = other.encryption_key_version_;
        encryption_algorithm_ = other.encryption_algorithm_;
        encrypted_data_ = std::move(other.encrypted_data_);
        decimal_unscaled_ = other.decimal_unscaled_;
        decimal_precision_ = other.decimal_precision_;
        decimal_scale_ = other.decimal_scale_;
        timezone_offset_seconds_ = other.timezone_offset_seconds_;

        if (other.is_null_) {
            other.is_encrypted_ = false;
            other.encryption_key_version_ = 0;
            other.encryption_algorithm_ = EncryptionAlgorithm::NONE;
            return;  // Nothing to move for NULL values
        }

        // Move primitive data (simple copy)
        std::memcpy(&data_, &other.data_, sizeof(data_));

        // Move string data
        string_data_ = std::move(other.string_data_);

        // Move binary data
        binary_data_ = std::move(other.binary_data_);

        // Move spatial data
        spatial_data_ = std::move(other.spatial_data_);

        // Move complex data
        complex_data_ = std::move(other.complex_data_);

        // Mark other as null
        other.is_null_ = true;
        other.type_ = DataType::NULL_TYPE;
        other.is_encrypted_ = false;
        other.encryption_key_version_ = 0;
        other.encryption_algorithm_ = EncryptionAlgorithm::NONE;
    }

    void TypedValue::clear()
    {
        // Clear string data
        string_data_.clear();

        // Clear binary data
        binary_data_.clear();

        // Clear encrypted payload
        encrypted_data_.clear();
        is_encrypted_ = false;
        encryption_key_version_ = 0;
        encryption_algorithm_ = EncryptionAlgorithm::NONE;

        // Clear spatial data
        spatial_data_.reset();

        // Clear complex data
        complex_data_.reset();

        // Reset to NULL
        is_null_ = true;
        type_ = DataType::NULL_TYPE;
        std::memset(&data_, 0, sizeof(data_));
        decimal_unscaled_ = 0;
        decimal_precision_ = 0;
        decimal_scale_ = 0;
        timezone_offset_seconds_ = 0;
    }

    // GeometryCollection equality operator implementation
    // Must be here (not in type_system.cpp) to avoid circular dependency with TypedValue
    bool GeometryCollection::operator==(const GeometryCollection& other) const
    {
        if (srid != other.srid) return false;
        if (geometries.size() != other.geometries.size()) return false;

        // Compare each geometry
        for (size_t i = 0; i < geometries.size(); ++i) {
            // Check if both pointers are null or both are valid
            bool this_null = (geometries[i] == nullptr);
            bool other_null = (other.geometries[i] == nullptr);

            if (this_null != other_null) return false;
            if (this_null) continue;  // Both are null, equal

            // Compare actual geometry values using TypedValue's operator==
            const TypedValue& this_geom = *geometries[i];
            const TypedValue& other_geom = *other.geometries[i];
            if (!(this_geom == other_geom)) return false;
        }

        return true;
    }

    // Type conversion implementation
    Status TypedValue::convertTo(const TypeInfo& target_type,
                                 TypedValue& result_out,
                                 CastFormat format,
                                 ErrorContext* ctx) const
    {
        DataType target = target_type.type;
        if (target == DataType::NULL_TYPE)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Cannot convert to NULL type");
            if (ctx)
            {
                ctx->violating_value = is_null_
                                           ? "NULL"
                                           : (is_encrypted_ ? "<encrypted>" : toString());
                if (ctx->message.find("target") == std::string::npos ||
                    ctx->message.find("value") == std::string::npos)
                {
                    ctx->message += " (value: '" + ctx->violating_value + "', target: NULL)";
                }
            }
            return Status::INVALID_ARGUMENT;
        }

        if (is_null_)
        {
            result_out = TypedValue(target);
            result_out.is_null_ = true;
            if (isDecimalLike(target))
            {
                if (target_type.precision != 0)
                {
                    result_out.decimal_precision_ = static_cast<uint8_t>(target_type.precision);
                }
                else if (target == DataType::DECIMAL)
                {
                    result_out.decimal_precision_ = DECIMAL_MAX_PRECISION;
                }
                else
                {
                    result_out.decimal_precision_ = defaultDecfloatPrecision(target);
                }
                result_out.decimal_scale_ = static_cast<uint8_t>(target_type.scale);
            }
            return Status::OK;
        }

        ensureDecrypted();

        std::string target_name = TypeSystem::getTypeName(target);
        std::string source_value;
        bool source_ready = false;
        auto getSourceValue = [&]() -> const std::string&
        {
            if (!source_ready)
            {
                source_value = toString();
                source_ready = true;
            }
            return source_value;
        };

        auto wrapStatus = [&](Status status) -> Status
        {
            if (status != Status::OK && ctx)
            {
                if (ctx->violating_value.empty())
                {
                    ctx->violating_value = getSourceValue();
                }
                if (ctx->hint.empty())
                {
                    ctx->hint = "target type: " + target_name;
                }
                if (!ctx->message.empty())
                {
                    if (ctx->message.find("target") == std::string::npos ||
                        ctx->message.find("value") == std::string::npos)
                    {
                        ctx->message += " (value: '" + getSourceValue() + "', target: " +
                                        target_name + ")";
                    }
                }
                else
                {
                    ctx->message = "Failed to cast value '" + getSourceValue() + "' to " +
                                   target_name;
                    ctx->code = status;
                    ctx->sqlstate = statusToSQLState(status);
                }
            }
            return status;
        };

        auto setStringResult = [&](DataType string_type, const std::string& value) -> Status
        {
            std::string adjusted = value;
            if ((string_type == DataType::CHAR || string_type == DataType::VARCHAR) &&
                target_type.precision > 0)
            {
                size_t char_count = UTF8Utils::countCharacters(adjusted);
                if (!adjusted.empty() && char_count == 0)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Invalid UTF-8 sequence");
                    return wrapStatus(Status::INVALID_ARGUMENT);
                }
                if (char_count > target_type.precision)
                {
                    SET_ERROR_CONTEXT(ctx, Status::STRING_DATA_RIGHT_TRUNCATION,
                                      "String value exceeds target length");
                    return wrapStatus(Status::STRING_DATA_RIGHT_TRUNCATION);
                }
                if (string_type == DataType::CHAR && char_count < target_type.precision)
                {
                    adjusted.append(target_type.precision - char_count, ' ');
                }
            }

            result_out = TypedValue(string_type);
            result_out.is_null_ = false;
            result_out.string_data_ = adjusted;
            return Status::OK;
        };

        auto setBinaryResult = [&](DataType binary_type, const std::vector<uint8_t>& data) -> Status
        {
            std::vector<uint8_t> adjusted = data;
            if (binary_type == DataType::BINARY && target_type.precision > 0)
            {
                if (adjusted.size() > target_type.precision)
                {
                    SET_ERROR_CONTEXT(ctx, Status::STRING_DATA_RIGHT_TRUNCATION,
                                      "Binary value exceeds target length");
                    return wrapStatus(Status::STRING_DATA_RIGHT_TRUNCATION);
                }
                if (adjusted.size() < target_type.precision)
                {
                    adjusted.resize(target_type.precision, 0x00);
                }
            }
            else if (binary_type == DataType::VARBINARY && target_type.precision > 0)
            {
                if (adjusted.size() > target_type.precision)
                {
                    SET_ERROR_CONTEXT(ctx, Status::STRING_DATA_RIGHT_TRUNCATION,
                                      "Binary value exceeds target length");
                    return wrapStatus(Status::STRING_DATA_RIGHT_TRUNCATION);
                }
            }
            result_out = TypedValue(binary_type);
            result_out.is_null_ = false;
            result_out.binary_data_ = std::move(adjusted);
            return Status::OK;
        };

        auto stringValueForParse = [&]() -> std::string
        {
            return (type_ == DataType::JSONB) ? toString() : string_data_;
        };

        auto normalized_format = (format == CastFormat::DEFAULT) ? CastFormat::HEX : format;

        // Same type (handle modifiers)
        if (type_ == target)
        {
            if (isDecimalLike(target))
            {
                uint8_t default_precision = (target == DataType::DECIMAL)
                                                ? DECIMAL_MAX_PRECISION
                                                : defaultDecfloatPrecision(target);
                uint8_t precision = target_type.precision == 0
                                        ? (decimal_precision_ == 0 ? default_precision
                                                                   : decimal_precision_)
                                        : static_cast<uint8_t>(target_type.precision);
                uint8_t scale = target_type.precision == 0 && target_type.scale == 0
                                    ? decimal_scale_
                                    : static_cast<uint8_t>(target_type.scale);

                if (scale > precision || precision > DECIMAL_MAX_PRECISION)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Invalid DECIMAL precision/scale");
                    return wrapStatus(Status::INVALID_ARGUMENT);
                }

                Decimal current(decimal_unscaled_,
                                decimal_precision_ == 0 ? precision : decimal_precision_,
                                decimal_scale_);
                Decimal adjusted = current.rescale(precision, scale, DecimalRoundingMode::HALF_UP);
                int128_t max_abs = POWERS_OF_10[precision] - 1;
                if (adjusted.unscaledValue() > max_abs ||
                    adjusted.unscaledValue() < -max_abs)
                {
                    SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                      "DECIMAL value out of range");
                    return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                }
                result_out = makeDecimal(adjusted.unscaledValue(), precision, scale);
                result_out.type_ = target;
                return Status::OK;
            }

            if (target == DataType::CHAR || target == DataType::VARCHAR)
            {
                return setStringResult(target, string_data_);
            }
            if (target == DataType::BINARY || target == DataType::VARBINARY)
            {
                return setBinaryResult(target, binary_data_);
            }

            result_out = *this;
            return Status::OK;
        }

        auto setInvalidNumber = [&](const std::string& input) -> Status
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                              (input + " is not a number").c_str());
            return wrapStatus(Status::INVALID_TEXT_REPRESENTATION);
        };

        auto readInt128Value = [&](int128_t& out_value) -> bool
        {
            if (binary_data_.size() != 16)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "INT128 storage size invalid");
                return false;
            }
            size_t offset = 0;
            std::vector<uint8_t> bytes = binary_data_;
            if (!readInt128(bytes, offset, 16, out_value))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "INT128 decode failed");
                return false;
            }
            return true;
        };

        auto readUInt128Value = [&](uint128_t& out_value) -> bool
        {
            if (binary_data_.size() != 16)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "UINT128 storage size invalid");
                return false;
            }
            size_t offset = 0;
            std::vector<uint8_t> bytes = binary_data_;
            if (!readUint128(bytes, offset, 16, out_value))
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "UINT128 decode failed");
                return false;
            }
            return true;
        };

        auto setUnsigned128Result = [&](uint128_t value) -> Status
        {
            result_out = TypedValue(DataType::UINT128);
            result_out.is_null_ = false;
            result_out.binary_data_.clear();
            appendUint128(result_out.binary_data_, value, 16);
            return Status::OK;
        };

        auto setIntegerResult = [&](DataType int_type, int128_t value) -> Status
        {
            if (isUnsignedType(int_type) && value < 0)
            {
                SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                  "Negative value for unsigned type");
                return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
            }

            switch (int_type)
            {
                case DataType::INT8:
                    if (value < std::numeric_limits<int8_t>::min() ||
                        value > std::numeric_limits<int8_t>::max())
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "INT8 value out of range");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    result_out = TypedValue(DataType::INT8);
                    result_out.is_null_ = false;
                    result_out.data_.int8_val = static_cast<int8_t>(value);
                    return Status::OK;
                case DataType::INT16:
                    if (value < std::numeric_limits<int16_t>::min() ||
                        value > std::numeric_limits<int16_t>::max())
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "INT16 value out of range");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    result_out = TypedValue(DataType::INT16);
                    result_out.is_null_ = false;
                    result_out.data_.int16_val = static_cast<int16_t>(value);
                    return Status::OK;
                case DataType::INT32:
                    if (value < std::numeric_limits<int32_t>::min() ||
                        value > std::numeric_limits<int32_t>::max())
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "INT32 value out of range");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    result_out = TypedValue(DataType::INT32);
                    result_out.is_null_ = false;
                    result_out.data_.int32_val = static_cast<int32_t>(value);
                    return Status::OK;
                case DataType::INT64:
                case DataType::MONEY:
                    if (value < std::numeric_limits<int64_t>::min() ||
                        value > std::numeric_limits<int64_t>::max())
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "INT64 value out of range");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    result_out = TypedValue(int_type == DataType::MONEY ? DataType::MONEY
                                                                        : DataType::INT64);
                    result_out.is_null_ = false;
                    result_out.data_.int64_val = static_cast<int64_t>(value);
                    return Status::OK;
                case DataType::INT128:
                {
                    result_out = TypedValue(DataType::INT128);
                    result_out.is_null_ = false;
                    result_out.binary_data_.clear();
                    appendInt128(result_out.binary_data_, value, 16);
                    return Status::OK;
                }
                case DataType::UINT128:
                {
                    result_out = TypedValue(DataType::UINT128);
                    result_out.is_null_ = false;
                    result_out.binary_data_.clear();
                    appendUint128(result_out.binary_data_, static_cast<uint128_t>(value), 16);
                    return Status::OK;
                }
                case DataType::UINT8:
                    if (value < 0 || static_cast<uint128_t>(value) >
                                     std::numeric_limits<uint8_t>::max())
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "UINT8 value out of range");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    result_out = TypedValue(DataType::UINT8);
                    result_out.is_null_ = false;
                    result_out.data_.uint8_val = static_cast<uint8_t>(value);
                    return Status::OK;
                case DataType::UINT16:
                    if (value < 0 || static_cast<uint128_t>(value) >
                                     std::numeric_limits<uint16_t>::max())
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "UINT16 value out of range");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    result_out = TypedValue(DataType::UINT16);
                    result_out.is_null_ = false;
                    result_out.data_.uint16_val = static_cast<uint16_t>(value);
                    return Status::OK;
                case DataType::UINT32:
                    if (value < 0 || static_cast<uint128_t>(value) >
                                     std::numeric_limits<uint32_t>::max())
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "UINT32 value out of range");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    result_out = TypedValue(DataType::UINT32);
                    result_out.is_null_ = false;
                    result_out.data_.uint32_val = static_cast<uint32_t>(value);
                    return Status::OK;
                case DataType::UINT64:
                    if (value < 0 || static_cast<uint128_t>(value) >
                                     std::numeric_limits<uint64_t>::max())
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "UINT64 value out of range");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    result_out = TypedValue(DataType::UINT64);
                    result_out.is_null_ = false;
                    result_out.data_.uint64_val = static_cast<uint64_t>(value);
                    return Status::OK;
                default:
                    break;
            }

            SET_ERROR_CONTEXT(ctx, Status::DATATYPE_MISMATCH, "Unsupported integer target");
            return wrapStatus(Status::DATATYPE_MISMATCH);
        };

        switch (target)
        {
            case DataType::JSONB:
            {
                if (type_ == DataType::JSONB)
                {
                    result_out = *this;
                    return Status::OK;
                }

                std::string text;
                if (isStringLike(type_))
                {
                    text = stringValueForParse();
                }
                else if (isBinaryLike(type_) || type_ == DataType::UUID || type_ == DataType::INT128 ||
                         type_ == DataType::UINT128)
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATATYPE_MISMATCH,
                                      "Cannot convert binary type to JSONB");
                    return wrapStatus(Status::DATATYPE_MISMATCH);
                }
                else
                {
                    text = toString();
                }

                std::vector<uint8_t> encoded;
                if (!encodeJsonb(text, encoded, ctx))
                {
                    return wrapStatus(Status::INVALID_TEXT_REPRESENTATION);
                }

                result_out = makeJSONB(encoded);
                return Status::OK;
            }
            case DataType::CHAR:
            case DataType::VARCHAR:
            case DataType::TEXT:
            case DataType::JSON:
            case DataType::XML:
            {
                std::string value;
                if (isBinaryLike(type_))
                {
                    const std::vector<uint8_t>& data = getBinary();
                    switch (normalized_format)
                    {
                        case CastFormat::HEX:
                            value = encodeHex(data);
                            break;
                        case CastFormat::BASE64:
                            value = encodeBase64(data);
                            break;
                        case CastFormat::ESCAPE:
                            value = encodeEscape(data);
                            break;
                        default:
                            SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED,
                                              "Unsupported binary cast format");
                            return wrapStatus(Status::NOT_SUPPORTED);
                    }
                }
                else if (isIntegerType(type_))
                {
                    if (format == CastFormat::HEX)
                    {
                        if (type_ == DataType::INT128)
                        {
                            int128_t val = 0;
                            if (!readInt128Value(val))
                            {
                                return wrapStatus(Status::INVALID_ARGUMENT);
                            }
                            value = formatSignedHex(val);
                        }
                        else if (type_ == DataType::UINT128)
                        {
                            uint128_t val = 0;
                            if (!readUInt128Value(val))
                            {
                                return wrapStatus(Status::INVALID_ARGUMENT);
                            }
                            value = formatUnsignedHex(val);
                        }
                        else if (isUnsignedType(type_))
                        {
                            uint128_t val = 0;
                            switch (type_)
                            {
                                case DataType::UINT8: val = data_.uint8_val; break;
                                case DataType::UINT16: val = data_.uint16_val; break;
                                case DataType::UINT32: val = data_.uint32_val; break;
                                case DataType::UINT64: val = data_.uint64_val; break;
                                default: break;
                            }
                            value = formatUnsignedHex(val);
                        }
                        else
                        {
                            value = formatSignedHex(static_cast<int128_t>(toInt64()));
                        }
                    }
                    else if (format == CastFormat::DEFAULT)
                    {
                        value = toString();
                    }
                    else
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED,
                                          "Unsupported numeric cast format");
                        return wrapStatus(Status::NOT_SUPPORTED);
                    }
                }
                else
                {
                    value = toString();
                }

                return setStringResult(target, value);
            }
            case DataType::BINARY:
            case DataType::VARBINARY:
            case DataType::BLOB:
            case DataType::BYTEA:
            case DataType::VECTOR:
            {
                if (isBinaryLike(type_) || type_ == DataType::UUID || type_ == DataType::INT128 ||
                    type_ == DataType::UINT128 || type_ == DataType::JSONB)
                {
                    return setBinaryResult(target, getBinary());
                }

                if (isStringLike(type_))
                {
                    std::vector<uint8_t> data;
                    std::string text = stringValueForParse();
                    bool ok = false;
                    switch (normalized_format)
                    {
                        case CastFormat::HEX:
                            ok = decodeHex(text, data, ctx);
                            break;
                        case CastFormat::BASE64:
                            ok = decodeBase64(text, data, ctx);
                            break;
                        case CastFormat::ESCAPE:
                            ok = decodeEscape(text, data, ctx);
                            break;
                        default:
                            SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED,
                                              "Unsupported binary cast format");
                            return wrapStatus(Status::NOT_SUPPORTED);
                    }
                    if (!ok)
                    {
                        return wrapStatus(Status::INVALID_TEXT_REPRESENTATION);
                    }
                    return setBinaryResult(target, data);
                }

                SET_ERROR_CONTEXT(ctx, Status::DATATYPE_MISMATCH,
                                  "Cannot convert to binary type");
                return wrapStatus(Status::DATATYPE_MISMATCH);
            }
            case DataType::UUID:
            {
                if (type_ == DataType::UUID)
                {
                    result_out = *this;
                    return Status::OK;
                }

                std::vector<uint8_t> uuid_bytes;
                if (isStringLike(type_))
                {
                    if (!parseUuidString(stringValueForParse(), uuid_bytes, ctx))
                    {
                        return wrapStatus(Status::INVALID_TEXT_REPRESENTATION);
                    }
                }
                else if (isBinaryLike(type_) || type_ == DataType::INT128 ||
                         type_ == DataType::UINT128)
                {
                    uuid_bytes = getBinary();
                    if (uuid_bytes.size() != 16)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                          "UUID requires 16 bytes");
                        return wrapStatus(Status::INVALID_TEXT_REPRESENTATION);
                    }
                }
                else
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATATYPE_MISMATCH,
                                      "Cannot convert to UUID");
                    return wrapStatus(Status::DATATYPE_MISMATCH);
                }

                result_out = makeUUID(uuid_bytes);
                return Status::OK;
            }
            case DataType::BOOLEAN:
            {
                bool value = false;
                if (type_ == DataType::BOOLEAN)
                {
                    value = data_.bool_val;
                }
                else if (isNumericType(type_))
                {
                    if (type_ == DataType::DECIMAL)
                    {
                        uint8_t precision = decimal_precision_ == 0
                                                ? DECIMAL_MAX_PRECISION
                                                : decimal_precision_;
                        Decimal dec(decimal_unscaled_, precision, decimal_scale_);
                        value = dec.unscaledValue() != 0;
                    }
                    else if (type_ == DataType::DECFLOAT16 ||
                             type_ == DataType::DECFLOAT34)
                    {
                        DecFloat df;
                        if (decodeDecfloat(binary_data_, type_, df, ctx) != Status::OK)
                        {
                            return wrapStatus(Status::INVALID_ARGUMENT);
                        }
                        value = !df.isZero();
                    }
                    else if (type_ == DataType::MONEY)
                    {
                        value = data_.int64_val != 0;
                    }
                    else if (type_ == DataType::INT128)
                    {
                        int128_t val = 0;
                        if (!readInt128Value(val))
                        {
                            return wrapStatus(Status::INVALID_ARGUMENT);
                        }
                        value = val != 0;
                    }
                    else if (type_ == DataType::UINT128)
                    {
                        uint128_t val = 0;
                        if (!readUInt128Value(val))
                        {
                            return wrapStatus(Status::INVALID_ARGUMENT);
                        }
                        value = val != 0;
                    }
                    else if (isFloatType(type_))
                    {
                        double val = (type_ == DataType::FLOAT32)
                                         ? static_cast<double>(data_.float32_val)
                                         : data_.float64_val;
                        value = val != 0.0;
                    }
                    else
                    {
                        value = toInt64() != 0;
                    }
                }
                else if (isStringLike(type_))
                {
                    std::string text = trimAscii(stringValueForParse());
                    std::string lower;
                    lower.reserve(text.size());
                    for (char ch : text)
                    {
                        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
                    }
                    if (lower == "true" || lower == "t" || lower == "1")
                    {
                        value = true;
                    }
                    else if (lower == "false" || lower == "f" || lower == "0")
                    {
                        value = false;
                    }
                    else
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_TEXT_REPRESENTATION,
                                          (text + " is not a boolean").c_str());
                        return wrapStatus(Status::INVALID_TEXT_REPRESENTATION);
                    }
                }
                else
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATATYPE_MISMATCH,
                                      "Cannot convert to BOOLEAN");
                    return wrapStatus(Status::DATATYPE_MISMATCH);
                }

                result_out = makeBool(value);
                return Status::OK;
            }
            case DataType::UINT128:
            {
                if (type_ == DataType::UINT128)
                {
                    result_out = *this;
                    return Status::OK;
                }

                uint128_t value = 0;
                if (type_ == DataType::INT128)
                {
                    int128_t signed_val = 0;
                    if (!readInt128Value(signed_val))
                    {
                        return wrapStatus(Status::INVALID_ARGUMENT);
                    }
                    if (signed_val < 0)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "Negative value for UINT128");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    value = static_cast<uint128_t>(signed_val);
                }
                else if (isUnsignedType(type_))
                {
                    switch (type_)
                    {
                        case DataType::UINT8: value = data_.uint8_val; break;
                        case DataType::UINT16: value = data_.uint16_val; break;
                        case DataType::UINT32: value = data_.uint32_val; break;
                        case DataType::UINT64: value = data_.uint64_val; break;
                        default: break;
                    }
                }
                else if (isIntegerType(type_))
                {
                    int128_t signed_val = 0;
                    if (type_ == DataType::INT128)
                    {
                        if (!readInt128Value(signed_val))
                        {
                            return wrapStatus(Status::INVALID_ARGUMENT);
                        }
                    }
                    else
                    {
                        signed_val = toInt64();
                    }
                    if (signed_val < 0)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "Negative value for UINT128");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    value = static_cast<uint128_t>(signed_val);
                }
                else if (type_ == DataType::DECIMAL)
                {
                    uint8_t precision = decimal_precision_ == 0
                                            ? DECIMAL_MAX_PRECISION
                                            : decimal_precision_;
                    Decimal dec(decimal_unscaled_, precision, decimal_scale_);
                    int128_t dec_val = dec.unscaledValue();
                    if (dec.scale() > 0)
                    {
                        dec_val /= POWERS_OF_10[dec.scale()];
                    }
                    if (dec_val < 0)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "Negative value for UINT128");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    value = static_cast<uint128_t>(dec_val);
                }
                else if (type_ == DataType::DECFLOAT16 ||
                         type_ == DataType::DECFLOAT34)
                {
                    DecFloat df;
                    if (decodeDecfloat(binary_data_, type_, df, ctx) != Status::OK)
                    {
                        return wrapStatus(Status::INVALID_ARGUMENT);
                    }
                    int128_t dec_val = 0;
                    Status st = decfloatToInt128(df, dec_val, ctx);
                    if (st != Status::OK)
                    {
                        return st;
                    }
                    if (dec_val < 0)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "Negative value for UINT128");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    value = static_cast<uint128_t>(dec_val);
                }
                else if (type_ == DataType::MONEY)
                {
                    Decimal dec(static_cast<int128_t>(data_.int64_val), 19, 4);
                    int128_t money_val = static_cast<int128_t>(dec.toInt64());
                    if (money_val < 0)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "Negative value for UINT128");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    value = static_cast<uint128_t>(money_val);
                }
                else if (isFloatType(type_))
                {
                    double val = (type_ == DataType::FLOAT32)
                                     ? static_cast<double>(data_.float32_val)
                                     : data_.float64_val;
                    if (!std::isfinite(val))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "Float is not finite");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    if (val < 0.0)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "Negative value for UINT128");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    long double max_uint128 = static_cast<long double>(~uint128_t{0});
                    if (static_cast<long double>(val) > max_uint128)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "UINT128 value out of range");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    value = static_cast<uint128_t>(val);
                }
                else if (type_ == DataType::BOOLEAN)
                {
                    value = data_.bool_val ? 1u : 0u;
                }
                else if (isStringLike(type_))
                {
                    if (format == CastFormat::BASE64 || format == CastFormat::ESCAPE)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED,
                                          "Unsupported numeric cast format");
                        return wrapStatus(Status::NOT_SUPPORTED);
                    }
                    std::string text = trimAscii(stringValueForParse());
                    uint128_t parsed = 0;
                    if (!parseUnsignedIntegerString(text, parsed, ctx, format))
                    {
                        return setInvalidNumber(text);
                    }
                    value = parsed;
                }
                else
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATATYPE_MISMATCH,
                                      "Cannot convert to UINT128");
                    return wrapStatus(Status::DATATYPE_MISMATCH);
                }

                return setUnsigned128Result(value);
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
            {
                int128_t value = 0;
                if (isIntegerType(type_))
                {
                    if (type_ == DataType::INT128)
                    {
                        if (!readInt128Value(value))
                        {
                            return wrapStatus(Status::INVALID_ARGUMENT);
                        }
                    }
                    else if (type_ == DataType::UINT128)
                    {
                        uint128_t val = 0;
                        if (!readUInt128Value(val))
                        {
                            return wrapStatus(Status::INVALID_ARGUMENT);
                        }
                        uint128_t max_signed = (uint128_t{1} << 127) - 1;
                        if (val > max_signed)
                        {
                            SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                              "UINT128 value out of range");
                            return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                        }
                        value = static_cast<int128_t>(val);
                    }
                    else if (isUnsignedType(type_))
                    {
                        switch (type_)
                        {
                            case DataType::UINT8: value = data_.uint8_val; break;
                            case DataType::UINT16: value = data_.uint16_val; break;
                            case DataType::UINT32: value = data_.uint32_val; break;
                            case DataType::UINT64: value = data_.uint64_val; break;
                            default: break;
                        }
                    }
                    else
                    {
                        value = toInt64();
                    }
                }
                else if (type_ == DataType::DECIMAL)
                {
                    uint8_t precision = decimal_precision_ == 0
                                            ? DECIMAL_MAX_PRECISION
                                            : decimal_precision_;
                    Decimal dec(decimal_unscaled_, precision, decimal_scale_);
                    int128_t dec_val = dec.unscaledValue();
                    if (dec.scale() > 0)
                    {
                        dec_val /= POWERS_OF_10[dec.scale()];
                    }
                    value = dec_val;
                }
                else if (type_ == DataType::DECFLOAT16 ||
                         type_ == DataType::DECFLOAT34)
                {
                    DecFloat df;
                    if (decodeDecfloat(binary_data_, type_, df, ctx) != Status::OK)
                    {
                        return wrapStatus(Status::INVALID_ARGUMENT);
                    }
                    Status st = decfloatToInt128(df, value, ctx);
                    if (st != Status::OK)
                    {
                        return st;
                    }
                }
                else if (type_ == DataType::MONEY)
                {
                    Decimal dec(static_cast<int128_t>(data_.int64_val), 19, 4);
                    value = static_cast<int128_t>(dec.toInt64());
                }
                else if (isFloatType(type_))
                {
                    double val = (type_ == DataType::FLOAT32)
                                     ? static_cast<double>(data_.float32_val)
                                     : data_.float64_val;
                    if (!std::isfinite(val))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "Float is not finite");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    value = static_cast<int128_t>(val);
                }
                else if (type_ == DataType::BOOLEAN)
                {
                    value = data_.bool_val ? 1 : 0;
                }
                else if (isStringLike(type_))
                {
                    if (format == CastFormat::BASE64 || format == CastFormat::ESCAPE)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NOT_SUPPORTED,
                                          "Unsupported numeric cast format");
                        return wrapStatus(Status::NOT_SUPPORTED);
                    }
                    int128_t parsed = 0;
                    std::string text = trimAscii(stringValueForParse());
                    bool allow_signed = !isUnsignedType(target);
                    if (!parseIntegerString(text, allow_signed, parsed, ctx, format))
                    {
                        return setInvalidNumber(text);
                    }
                    value = parsed;
                }
                else
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATATYPE_MISMATCH,
                                      "Cannot convert to integer type");
                    return wrapStatus(Status::DATATYPE_MISMATCH);
                }

                return setIntegerResult(target, value);
            }
            case DataType::FLOAT32:
            case DataType::FLOAT64:
            {
                double value = 0.0;
                bool source_is_float = isFloatType(type_);
                if (isFloatType(type_))
                {
                    value = (type_ == DataType::FLOAT32)
                                ? static_cast<double>(data_.float32_val)
                                : data_.float64_val;
                }
                else if (isIntegerType(type_))
                {
                    if (type_ == DataType::INT128)
                    {
                        int128_t val = 0;
                        if (!readInt128Value(val))
                        {
                            return wrapStatus(Status::INVALID_ARGUMENT);
                        }
                        value = static_cast<double>(val);
                    }
                    else if (type_ == DataType::UINT128)
                    {
                        uint128_t val = 0;
                        if (!readUInt128Value(val))
                        {
                            return wrapStatus(Status::INVALID_ARGUMENT);
                        }
                        value = static_cast<double>(val);
                    }
                    else if (isUnsignedType(type_))
                    {
                        switch (type_)
                        {
                            case DataType::UINT8: value = data_.uint8_val; break;
                            case DataType::UINT16: value = data_.uint16_val; break;
                            case DataType::UINT32: value = data_.uint32_val; break;
                            case DataType::UINT64: value = static_cast<double>(data_.uint64_val); break;
                            default: break;
                        }
                    }
                    else
                    {
                        value = static_cast<double>(toInt64());
                    }
                }
                else if (type_ == DataType::DECIMAL)
                {
                    uint8_t precision = decimal_precision_ == 0
                                            ? DECIMAL_MAX_PRECISION
                                            : decimal_precision_;
                    Decimal dec(decimal_unscaled_, precision, decimal_scale_);
                    value = dec.toDouble();
                }
                else if (type_ == DataType::DECFLOAT16 ||
                         type_ == DataType::DECFLOAT34)
                {
                    DecFloat df;
                    if (decodeDecfloat(binary_data_, type_, df, ctx) != Status::OK)
                    {
                        return wrapStatus(Status::INVALID_ARGUMENT);
                    }
                    try
                    {
                        value = std::stod(df.toString());
                    }
                    catch (...)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "DECFLOAT value out of range");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                }
                else if (type_ == DataType::MONEY)
                {
                    Decimal dec(static_cast<int128_t>(data_.int64_val), 19, 4);
                    value = dec.toDouble();
                }
                else if (type_ == DataType::BOOLEAN)
                {
                    value = data_.bool_val ? 1.0 : 0.0;
                }
                else if (isStringLike(type_))
                {
                    std::string text = trimAscii(stringValueForParse());
                    if (!parseFloatingString(text, value, ctx))
                    {
                        return setInvalidNumber(text);
                    }
                }
                else
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATATYPE_MISMATCH,
                                      "Cannot convert to float");
                    return wrapStatus(Status::DATATYPE_MISMATCH);
                }

                if (!std::isfinite(value))
                {
                    if (source_is_float)
                    {
                        if (target == DataType::FLOAT32)
                        {
                            result_out = makeFloat32(static_cast<float>(value));
                        }
                        else
                        {
                            result_out = makeFloat64(value);
                        }
                        return Status::OK;
                    }
                    SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                      "Float is not finite");
                    return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                }

                if (target == DataType::FLOAT32)
                {
                    if (std::fabs(value) > std::numeric_limits<float>::max())
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "FLOAT32 value out of range");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    result_out = makeFloat32(static_cast<float>(value));
                }
                else
                {
                    result_out = makeFloat64(value);
                }
                return Status::OK;
            }
            case DataType::DECIMAL:
            case DataType::DECFLOAT16:
            case DataType::DECFLOAT34:
            {
                if (target == DataType::DECIMAL)
                {
                    uint8_t precision = target_type.precision == 0
                                            ? DECIMAL_MAX_PRECISION
                                            : static_cast<uint8_t>(target_type.precision);
                    uint8_t scale = static_cast<uint8_t>(target_type.scale);
                    if (scale > precision || precision > DECIMAL_MAX_PRECISION)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Invalid DECIMAL precision/scale");
                        return wrapStatus(Status::INVALID_ARGUMENT);
                    }

                    Decimal dec;
                    if (type_ == DataType::DECIMAL)
                    {
                        Decimal current(decimal_unscaled_,
                                        decimal_precision_ == 0
                                            ? DECIMAL_MAX_PRECISION
                                            : decimal_precision_,
                                        decimal_scale_);
                        dec = current.rescale(precision, scale, DecimalRoundingMode::HALF_UP);
                    }
                    else if (type_ == DataType::DECFLOAT16 ||
                             type_ == DataType::DECFLOAT34)
                    {
                        DecFloat df;
                        if (decodeDecfloat(binary_data_, type_, df, ctx) != Status::OK)
                        {
                            return wrapStatus(Status::INVALID_ARGUMENT);
                        }
                        Status st = decfloatToDecimal(df, precision, scale, dec, ctx);
                        if (st != Status::OK)
                        {
                            return st;
                        }
                    }
                    else if (type_ == DataType::MONEY)
                    {
                        Decimal money(static_cast<int128_t>(data_.int64_val), 19, 4);
                        dec = money.rescale(precision, scale, DecimalRoundingMode::HALF_UP);
                    }
                    else if (isIntegerType(type_) || type_ == DataType::BOOLEAN)
                    {
                        int128_t int_val = 0;
                        if (type_ == DataType::INT128)
                        {
                            if (!readInt128Value(int_val))
                            {
                                return wrapStatus(Status::INVALID_ARGUMENT);
                            }
                        }
                        else if (type_ == DataType::UINT128)
                        {
                            uint128_t val = 0;
                            if (!readUInt128Value(val))
                            {
                                return wrapStatus(Status::INVALID_ARGUMENT);
                            }
                            uint128_t max_signed = (uint128_t{1} << 127) - 1;
                            if (val > max_signed)
                            {
                                SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                                  "UINT128 value out of range");
                                return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                            }
                            int_val = static_cast<int128_t>(val);
                        }
                        else if (isUnsignedType(type_))
                        {
                            switch (type_)
                            {
                                case DataType::UINT8: int_val = data_.uint8_val; break;
                                case DataType::UINT16: int_val = data_.uint16_val; break;
                                case DataType::UINT32: int_val = data_.uint32_val; break;
                                case DataType::UINT64: int_val = data_.uint64_val; break;
                                default: break;
                            }
                        }
                        else if (type_ == DataType::BOOLEAN)
                        {
                            int_val = data_.bool_val ? 1 : 0;
                        }
                        else
                        {
                            int_val = toInt64();
                        }

                        int128_t scaled = int_val * POWERS_OF_10[scale];
                        dec = Decimal(scaled, precision, scale);
                    }
                    else if (isFloatType(type_))
                    {
                        double val = (type_ == DataType::FLOAT32)
                                         ? static_cast<double>(data_.float32_val)
                                         : data_.float64_val;
                        if (!std::isfinite(val))
                        {
                            SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                              "Float is not finite");
                            return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                        }
                        dec = Decimal(val, precision, scale);
                    }
                    else if (isStringLike(type_))
                    {
                        std::string text = trimAscii(stringValueForParse());
                        Status status = Decimal::parseWithError(text, precision, scale, &dec, ctx);
                        if (status != Status::OK)
                        {
                            return setInvalidNumber(text);
                        }
                    }
                    else
                    {
                        SET_ERROR_CONTEXT(ctx, Status::DATATYPE_MISMATCH,
                                          "Cannot convert to DECIMAL");
                        return wrapStatus(Status::DATATYPE_MISMATCH);
                    }

                    int128_t max_abs = POWERS_OF_10[precision] - 1;
                    if (dec.unscaledValue() > max_abs || dec.unscaledValue() < -max_abs)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "DECIMAL value out of range");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }

                    result_out = makeDecimal(dec.unscaledValue(), precision, scale);
                    result_out.type_ = target;
                    return Status::OK;
                }

                DecFloat df;
                Status st = coerceToDecfloat(*this, target, df, ctx);
                if (st != Status::OK)
                {
                    return st;
                }
                std::vector<uint8_t> bytes;
                st = encodeDecfloat(df, target, bytes, ctx);
                if (st != Status::OK)
                {
                    return st;
                }
                result_out = makeDecfloat(target, bytes);
                return Status::OK;
            }
            case DataType::MONEY:
            {
                Decimal money;
                if (type_ == DataType::MONEY)
                {
                    result_out = *this;
                    return Status::OK;
                }
                if (type_ == DataType::DECIMAL)
                {
                    uint8_t precision = decimal_precision_ == 0
                                            ? DECIMAL_MAX_PRECISION
                                            : decimal_precision_;
                    Decimal dec(decimal_unscaled_, precision, decimal_scale_);
                    money = dec.rescale(19, 4, DecimalRoundingMode::HALF_UP);
                }
                else if (type_ == DataType::DECFLOAT16 ||
                         type_ == DataType::DECFLOAT34)
                {
                    DecFloat df;
                    if (decodeDecfloat(binary_data_, type_, df, ctx) != Status::OK)
                    {
                        return wrapStatus(Status::INVALID_ARGUMENT);
                    }
                    Decimal dec;
                    Status st = decfloatToDecimal(df, 19, 4, dec, ctx);
                    if (st != Status::OK)
                    {
                        return st;
                    }
                    money = dec.rescale(19, 4, DecimalRoundingMode::HALF_UP);
                }
                else if (isIntegerType(type_) || type_ == DataType::BOOLEAN)
                {
                    int128_t int_val = 0;
                    if (type_ == DataType::INT128)
                    {
                        if (!readInt128Value(int_val))
                        {
                            return wrapStatus(Status::INVALID_ARGUMENT);
                        }
                    }
                    else if (type_ == DataType::UINT128)
                    {
                        uint128_t val = 0;
                        if (!readUInt128Value(val))
                        {
                            return wrapStatus(Status::INVALID_ARGUMENT);
                        }
                        uint128_t max_signed = (uint128_t{1} << 127) - 1;
                        if (val > max_signed)
                        {
                            SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                              "UINT128 value out of range");
                            return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                        }
                        int_val = static_cast<int128_t>(val);
                    }
                    else if (isUnsignedType(type_))
                    {
                        switch (type_)
                        {
                            case DataType::UINT8: int_val = data_.uint8_val; break;
                            case DataType::UINT16: int_val = data_.uint16_val; break;
                            case DataType::UINT32: int_val = data_.uint32_val; break;
                            case DataType::UINT64: int_val = data_.uint64_val; break;
                            default: break;
                        }
                    }
                    else if (type_ == DataType::BOOLEAN)
                    {
                        int_val = data_.bool_val ? 1 : 0;
                    }
                    else
                    {
                        int_val = toInt64();
                    }
                    int128_t scaled = int_val * POWERS_OF_10[4];
                    money = Decimal(scaled, 19, 4);
                }
                else if (isFloatType(type_))
                {
                    double val = (type_ == DataType::FLOAT32)
                                     ? static_cast<double>(data_.float32_val)
                                     : data_.float64_val;
                    if (!std::isfinite(val))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "Float is not finite");
                        return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                    }
                    money = Decimal(val, 19, 4);
                }
                else if (isStringLike(type_))
                {
                    std::string text = trimAscii(stringValueForParse());
                    Status status = Decimal::parseWithError(text, 19, 4, &money, ctx);
                    if (status != Status::OK)
                    {
                        return setInvalidNumber(text);
                    }
                }
                else
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATATYPE_MISMATCH,
                                      "Cannot convert to MONEY");
                    return wrapStatus(Status::DATATYPE_MISMATCH);
                }

                if (money.unscaledValue() < std::numeric_limits<int64_t>::min() ||
                    money.unscaledValue() > std::numeric_limits<int64_t>::max())
                {
                    SET_ERROR_CONTEXT(ctx, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                      "MONEY value out of range");
                    return wrapStatus(Status::NUMERIC_VALUE_OUT_OF_RANGE);
                }

                result_out = TypedValue(DataType::MONEY);
                result_out.is_null_ = false;
                result_out.data_.int64_val = static_cast<int64_t>(money.unscaledValue());
                return Status::OK;
            }
            case DataType::DATE:
            case DataType::TIME:
            case DataType::TIMESTAMP:
            {
                auto resolve_target_offset = [&](int64_t utc_micros) -> int32_t
                {
                    if (!target_type.with_timezone && target_type.timezone_hint == 0)
                    {
                        return 0;
                    }

                    if (target_type.timezone_hint != 0)
                    {
                        uint16_t tz_id = target_type.timezone_hint;
                        TimezoneOffset offset = timezoneManager().getOffset(tz_id, utc_micros);
                        return static_cast<int32_t>(offset.offset_minutes) * 60;
                    }

                    if (timezone_offset_seconds_ != 0)
                    {
                        return timezone_offset_seconds_;
                    }

                    uint16_t tz_id = timezoneManager().getDefaultTimezone();
                    TimezoneOffset offset = timezoneManager().getOffset(tz_id, utc_micros);
                    return static_cast<int32_t>(offset.offset_minutes) * 60;
                };

                if (type_ == DataType::DATE && target == DataType::TIMESTAMP)
                {
                    int64_t default_micros = defaultDateTimeMicros();
                    int64_t micros = data_.int64_val * FirebirdDateTime::SECONDS_PER_DAY *
                                         1000000 +
                                     default_micros;
                    int32_t target_offset = resolve_target_offset(micros);
                    result_out = makeTimestamp(micros, target_offset);
                    return Status::OK;
                }
                if (type_ == DataType::TIME && target == DataType::TIMESTAMP)
                {
                    int64_t micros = data_.int64_val;
                    int32_t target_offset = resolve_target_offset(micros);
                    result_out = makeTimestamp(micros, target_offset);
                    return Status::OK;
                }
                if (type_ == DataType::TIMESTAMP && target == DataType::DATE)
                {
                    int64_t utc_micros = data_.int64_val;
                    int32_t target_offset = resolve_target_offset(utc_micros);
                    int64_t local_micros = utc_micros +
                                           static_cast<int64_t>(target_offset) * 1000000;
                    int64_t local_seconds = local_micros / 1000000;
                    int64_t days = floorDiv(local_seconds, FirebirdDateTime::SECONDS_PER_DAY);
                    result_out = makeDate(days, target_offset);
                    return Status::OK;
                }
                if (type_ == DataType::TIMESTAMP && target == DataType::TIME)
                {
                    int64_t utc_micros = data_.int64_val;
                    int32_t target_offset = resolve_target_offset(utc_micros);
                    int64_t local_micros = utc_micros +
                                           static_cast<int64_t>(target_offset) * 1000000;
                    int64_t micros_per_day =
                        static_cast<int64_t>(FirebirdDateTime::SECONDS_PER_DAY) * 1000000;
                    int64_t time_micros = local_micros % micros_per_day;
                    if (time_micros < 0)
                    {
                        time_micros += micros_per_day;
                    }
                    result_out = makeTime(time_micros, target_offset);
                    return Status::OK;
                }

                if (!isStringLike(type_))
                {
                    SET_ERROR_CONTEXT(ctx, Status::DATATYPE_MISMATCH,
                                      "Temporal cast expects string input");
                    return wrapStatus(Status::DATATYPE_MISMATCH);
                }

                std::string base;
                int32_t offset_seconds = 0;
                bool has_offset = false;
                size_t min_pos = 0;
                if (target == DataType::DATE)
                {
                    min_pos = 10;
                }
                else if (target == DataType::TIME)
                {
                    min_pos = 5;
                }
                else
                {
                    min_pos = 16;
                }

                std::string input = stringValueForParse();
                if (!parseOffsetSuffix(input, min_pos, base, offset_seconds, has_offset, ctx))
                {
                    return wrapStatus(Status::INVALID_DATETIME_FORMAT);
                }

                if (target == DataType::DATE)
                {
                    int year = 0;
                    int month = 0;
                    int day = 0;
                    if (!parseDateParts(base, year, month, day))
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_DATETIME_FORMAT,
                                          "Invalid DATE format");
                        return wrapStatus(Status::INVALID_DATETIME_FORMAT);
                    }
                    int32_t mjd = FirebirdDateTime::dateToMJD(year, month, day);
                    int64_t days = static_cast<int64_t>(mjd) - FirebirdDateTime::UNIX_EPOCH_MJD;
                    int64_t default_micros = defaultDateTimeMicros();
                    int64_t local_micros = days * FirebirdDateTime::SECONDS_PER_DAY *
                                               1000000 +
                                           default_micros;
                    bool use_timezone = target_type.with_timezone ||
                                        target_type.timezone_hint != 0;
                    if (!has_offset && use_timezone)
                    {
                        uint16_t tz_id = target_type.timezone_hint != 0
                                             ? target_type.timezone_hint
                                             : timezoneManager().getDefaultTimezone();
                        offset_seconds = resolveTimezoneOffsetSeconds(tz_id, local_micros);
                    }
                    int64_t utc_micros = local_micros -
                                         static_cast<int64_t>(offset_seconds) * 1000000;
                    int64_t utc_days = floorDiv(utc_micros / 1000000,
                                                FirebirdDateTime::SECONDS_PER_DAY);
                    int32_t stored_offset = (has_offset || use_timezone) ? offset_seconds : 0;
                    result_out = makeDate(utc_days, stored_offset);
                    return Status::OK;
                }

                if (target == DataType::TIME)
                {
                    int hour = 0;
                    int minute = 0;
                    int second = 0;
                    int micros = 0;
                    if (!parseTimeParts(base, hour, minute, second, micros, ctx))
                    {
                        return wrapStatus(Status::INVALID_DATETIME_FORMAT);
                    }
                    if (minute < 0 || minute > 59 || second < 0 || second > 59 ||
                        hour < 0 || hour > 23)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_DATETIME_FORMAT,
                                          "Invalid TIME value");
                        return wrapStatus(Status::INVALID_DATETIME_FORMAT);
                    }
                    int64_t local_micros = (static_cast<int64_t>(hour) * 3600 +
                                            static_cast<int64_t>(minute) * 60 +
                                            static_cast<int64_t>(second)) * 1000000 +
                                           micros;
                    bool use_timezone = target_type.with_timezone ||
                                        target_type.timezone_hint != 0;
                    if (!has_offset && use_timezone)
                    {
                        uint16_t tz_id = target_type.timezone_hint != 0
                                             ? target_type.timezone_hint
                                             : timezoneManager().getDefaultTimezone();
                        offset_seconds = resolveTimezoneOffsetSeconds(tz_id, local_micros);
                    }
                    int64_t utc_micros = local_micros -
                                         static_cast<int64_t>(offset_seconds) * 1000000;
                    int64_t micros_per_day =
                        static_cast<int64_t>(FirebirdDateTime::SECONDS_PER_DAY) * 1000000;
                    utc_micros %= micros_per_day;
                    if (utc_micros < 0)
                    {
                        utc_micros += micros_per_day;
                    }
                    int32_t stored_offset = (has_offset || use_timezone) ? offset_seconds : 0;
                    result_out = makeTime(utc_micros, stored_offset);
                    return Status::OK;
                }

                // TIMESTAMP
                size_t sep_pos = base.find('T');
                if (sep_pos == std::string::npos)
                {
                    sep_pos = base.find(' ');
                }
                if (sep_pos == std::string::npos)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_DATETIME_FORMAT,
                                      "Invalid TIMESTAMP format");
                    return wrapStatus(Status::INVALID_DATETIME_FORMAT);
                }
                std::string date_part = base.substr(0, sep_pos);
                std::string time_part = base.substr(sep_pos + 1);

                int year = 0;
                int month = 0;
                int day = 0;
                if (!parseDateParts(date_part, year, month, day))
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_DATETIME_FORMAT,
                                      "Invalid TIMESTAMP date");
                    return wrapStatus(Status::INVALID_DATETIME_FORMAT);
                }
                int hour = 0;
                int minute = 0;
                int second = 0;
                int micros = 0;
                if (!parseTimeParts(time_part, hour, minute, second, micros, ctx))
                {
                    return wrapStatus(Status::INVALID_DATETIME_FORMAT);
                }
                if (minute < 0 || minute > 59 || second < 0 || second > 59 ||
                    hour < 0 || hour > 23)
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_DATETIME_FORMAT,
                                      "Invalid TIMESTAMP time");
                    return wrapStatus(Status::INVALID_DATETIME_FORMAT);
                }

                int32_t mjd = FirebirdDateTime::dateToMJD(year, month, day);
                int64_t days = static_cast<int64_t>(mjd) - FirebirdDateTime::UNIX_EPOCH_MJD;
                int64_t local_micros = (days * FirebirdDateTime::SECONDS_PER_DAY +
                                        (hour * 3600 + minute * 60 + second)) * 1000000 +
                                       micros;
                bool use_timezone = target_type.with_timezone ||
                                    target_type.timezone_hint != 0;
                if (!has_offset && use_timezone)
                {
                    uint16_t tz_id = target_type.timezone_hint != 0
                                         ? target_type.timezone_hint
                                         : timezoneManager().getDefaultTimezone();
                    offset_seconds = resolveTimezoneOffsetSeconds(tz_id, local_micros);
                }
                int64_t utc_micros = local_micros -
                                     static_cast<int64_t>(offset_seconds) * 1000000;
                int32_t stored_offset = (has_offset || use_timezone) ? offset_seconds : 0;
                result_out = makeTimestamp(utc_micros, stored_offset);
                return Status::OK;
            }
            default:
                break;
        }

        SET_ERROR_CONTEXT(ctx, Status::DATATYPE_MISMATCH, "Unsupported type conversion");
        return wrapStatus(Status::DATATYPE_MISMATCH);
    }

    TypedValue TypedValue::convertTo(DataType target_type) const
    {
        TypedValue result;
        ErrorContext ctx;
        TypeInfo info(target_type);
        Status status = convertTo(info, result, CastFormat::DEFAULT, &ctx);
        if (status != Status::OK)
        {
            throw std::runtime_error(ctx.message.empty() ? "Type conversion failed"
                                                         : ctx.message);
        }
        return result;
    }

} // namespace scratchbird::core
