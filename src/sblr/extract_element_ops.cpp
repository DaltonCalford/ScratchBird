/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/sblr/extract_element_ops.h"
#include "scratchbird/sblr/extract_element_catalog.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/decimal.h"
#include "scratchbird/core/firebird_datetime.h"
#include "scratchbird/core/type_extractor.h"
#include "scratchbird/core/utf8_utils.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstring>
#include <limits>
#include <optional>
#include <sstream>
#include <unordered_set>

#ifdef HAVE_LIBXML2
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#endif

#include <openssl/md5.h>
#include <openssl/sha.h>

namespace scratchbird::sblr
{
    using json = nlohmann::json;
    using OrderedJson = nlohmann::ordered_json;

    namespace
    {
        constexpr int64_t kMicrosPerSecond = 1000000LL;
        constexpr int64_t kSecondsPerDay = 86400LL;
        constexpr int64_t kMicrosPerDay = kSecondsPerDay * kMicrosPerSecond;

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

        int64_t defaultDateTimeMicros()
        {
            core::Config &cfg = core::Config::getInstance();
            std::string default_time = cfg.getString("server.time", "date_default_time",
                                                     "00:00:00");
            int hour = 0;
            int minute = 0;
            int second = 0;
            int micros = 0;
            std::string time_part = default_time;
            std::string frac_part;
            size_t dot_pos = default_time.find('.');
            if (dot_pos != std::string::npos)
            {
                time_part = default_time.substr(0, dot_pos);
                frac_part = default_time.substr(dot_pos + 1);
            }
            int parsed = std::sscanf(time_part.c_str(), "%d:%d:%d", &hour, &minute, &second);
            if (parsed < 2)
            {
                return 0;
            }
            if (parsed == 2)
            {
                second = 0;
            }
            if (!frac_part.empty())
            {
                if (frac_part.size() > 6)
                {
                    return 0;
                }
                int frac_value = 0;
                for (char ch : frac_part)
                {
                    if (ch < '0' || ch > '9')
                    {
                        return 0;
                    }
                    frac_value = frac_value * 10 + (ch - '0');
                }
                int scale = 6 - static_cast<int>(frac_part.size());
                for (int i = 0; i < scale; ++i)
                {
                    frac_value *= 10;
                }
                micros = frac_value;
            }
            return (static_cast<int64_t>(hour) * 3600 +
                    static_cast<int64_t>(minute) * 60 +
                    static_cast<int64_t>(second)) * kMicrosPerSecond +
                   micros;
        }

        bool isValidDate(int32_t year, int32_t month, int32_t day)
        {
            if (month < 1 || month > 12)
            {
                return false;
            }
            static const int32_t days_in_month[] = {
                31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
            };
            int32_t dim = days_in_month[month - 1];
            if (month == 2)
            {
                bool leap = (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
                if (leap)
                {
                    dim = 29;
                }
            }
            return day >= 1 && day <= dim;
        }

        int64_t localDaysFromDate(int64_t stored_days, int32_t offset_seconds)
        {
            int64_t default_micros = defaultDateTimeMicros();
            int64_t adjust = floorDiv(default_micros -
                                          static_cast<int64_t>(offset_seconds) * kMicrosPerSecond,
                                      kMicrosPerDay);
            return stored_days - adjust;
        }

        int64_t utcDaysFromLocalDate(int32_t year, int32_t month, int32_t day,
                                     int32_t offset_seconds)
        {
            int64_t local_days = core::TypeExtractor::ymdToDays(year, month, day);
            int64_t local_micros = local_days * kMicrosPerDay + defaultDateTimeMicros();
            int64_t utc_micros = local_micros -
                                 static_cast<int64_t>(offset_seconds) * kMicrosPerSecond;
            return floorDiv(utc_micros / kMicrosPerSecond, kSecondsPerDay);
        }

        int64_t normalizeTimeOfDay(int64_t micros)
        {
            int64_t result = micros % kMicrosPerDay;
            if (result < 0)
            {
                result += kMicrosPerDay;
            }
            return result;
        }

        void splitLocalTime(int64_t local_micros, int32_t &hour, int32_t &minute,
                            int32_t &second, int32_t &micro)
        {
            int64_t total_seconds = floorDiv(local_micros, kMicrosPerSecond);
            int64_t micros_part = local_micros - total_seconds * kMicrosPerSecond;
            if (micros_part < 0)
            {
                micros_part += kMicrosPerSecond;
                total_seconds -= 1;
            }
            int64_t seconds_of_day = total_seconds % kSecondsPerDay;
            if (seconds_of_day < 0)
            {
                seconds_of_day += kSecondsPerDay;
            }
            hour = static_cast<int32_t>(seconds_of_day / 3600);
            minute = static_cast<int32_t>((seconds_of_day % 3600) / 60);
            second = static_cast<int32_t>(seconds_of_day % 60);
            micro = static_cast<int32_t>(micros_part);
        }

        bool parseIntegerArg(const std::vector<core::TypedValue>& args,
                             size_t index,
                             int64_t &out,
                             std::string *error)
        {
            if (index >= args.size())
            {
                if (error) *error = "Missing selector argument";
                return false;
            }
            if (args[index].isNull())
            {
                if (error) *error = "Selector argument cannot be NULL";
                return false;
            }
            out = args[index].toInt64();
            return true;
        }

        bool parseStringArg(const std::vector<core::TypedValue>& args,
                            size_t index,
                            std::string &out,
                            std::string *error)
        {
            if (index >= args.size())
            {
                if (error) *error = "Missing selector argument";
                return false;
            }
            if (args[index].isNull())
            {
                if (error) *error = "Selector argument cannot be NULL";
                return false;
            }
            out = args[index].toString();
            return true;
        }

        bool decodeBinaryValue(const core::TypedValue &value,
                               std::vector<uint8_t> &out,
                               std::string *error)
        {
            if (value.isNull())
            {
                if (error) *error = "Binary value cannot be NULL";
                return false;
            }
            switch (value.type())
            {
                case core::DataType::BINARY:
                case core::DataType::VARBINARY:
                case core::DataType::BLOB:
                case core::DataType::BYTEA:
                case core::DataType::UUID:
                case core::DataType::INT128:
                case core::DataType::UINT128:
                case core::DataType::JSONB:
                case core::DataType::VECTOR:
                    out = value.getBinary();
                    return true;
                default:
                    break;
            }
            if (error) *error = "Expected binary value";
            return false;
        }

        bool decodeVectorValue(const core::TypedValue &value,
                               std::vector<float> &out,
                               std::string *error)
        {
            std::vector<uint8_t> bytes;
            if (!decodeBinaryValue(value, bytes, error))
            {
                return false;
            }
            if (bytes.size() % sizeof(float) != 0)
            {
                if (error) *error = "Invalid vector payload";
                return false;
            }
            size_t count = bytes.size() / sizeof(float);
            out.resize(count);
            std::memcpy(out.data(), bytes.data(), bytes.size());
            return true;
        }

        OrderedJson canonicalizeJson(const json &input)
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
                for (const auto &key : keys)
                {
                    obj[key] = canonicalizeJson(input.at(key));
                }
                return obj;
            }
            if (input.is_array())
            {
                OrderedJson arr = OrderedJson::array();
                for (const auto &elem : input)
                {
                    arr.push_back(canonicalizeJson(elem));
                }
                return arr;
            }
            return input;
        }

        bool parseJson(const core::TypedValue &value, json &out, std::string *error)
        {
            try
            {
                if (value.type() == core::DataType::JSON)
                {
                    out = json::parse(value.toString());
                    return true;
                }
                if (value.type() == core::DataType::JSONB)
                {
                    out = json::from_cbor(value.getBinary());
                    return true;
                }
            }
            catch (const std::exception &ex)
            {
                if (error) *error = ex.what();
                return false;
            }
            if (error) *error = "Expected JSON or JSONB value";
            return false;
        }

        core::TypedValue jsonToValue(const json &j, core::DataType output_type)
        {
            if (output_type == core::DataType::JSONB)
            {
                OrderedJson canonical = canonicalizeJson(j);
                std::vector<uint8_t> encoded = OrderedJson::to_cbor(canonical);
                return core::TypedValue::makeJSONB(encoded);
            }
            if (output_type == core::DataType::JSON)
            {
                return core::TypedValue::makeJSON(j.dump());
            }
            return core::TypedValue::makeText(j.dump());
        }

        json valueToJson(const core::TypedValue &value)
        {
            if (value.isNull())
            {
                return json();
            }
            switch (value.type())
            {
                case core::DataType::INT8:
                case core::DataType::INT16:
                case core::DataType::INT32:
                case core::DataType::INT64:
                    return value.toInt64();
                case core::DataType::UINT8:
                case core::DataType::UINT16:
                case core::DataType::UINT32:
                case core::DataType::UINT64:
                    return value.getUInt64();
                case core::DataType::FLOAT32:
                case core::DataType::FLOAT64:
                case core::DataType::DECIMAL:
                case core::DataType::DECFLOAT16:
                case core::DataType::DECFLOAT34:
                    return value.toDouble();
                case core::DataType::BOOLEAN:
                    return value.getBoolean();
                case core::DataType::VARCHAR:
                case core::DataType::TEXT:
                case core::DataType::CHAR:
                case core::DataType::JSON:
                case core::DataType::XML:
                    return value.toString();
                case core::DataType::JSONB:
                {
                    json out;
                    if (parseJson(value, out, nullptr))
                    {
                        return out;
                    }
                    return json();
                }
                default:
                    return value.toString();
            }
        }

        std::vector<std::string> parseJsonPath(const std::string &path)
        {
            std::vector<std::string> components;
            if (path.empty())
            {
                return components;
            }
            if (path[0] == '$')
            {
                std::string current;
                bool in_bracket = false;
                for (size_t i = 1; i < path.length(); i++)
                {
                    char c = path[i];
                    if (c == '.' && !in_bracket)
                    {
                        if (!current.empty())
                        {
                            components.push_back(current);
                            current.clear();
                        }
                    }
                    else if (c == '[')
                    {
                        if (!current.empty())
                        {
                            components.push_back(current);
                            current.clear();
                        }
                        in_bracket = true;
                    }
                    else if (c == ']')
                    {
                        if (!current.empty())
                        {
                            components.push_back(current);
                            current.clear();
                        }
                        in_bracket = false;
                    }
                    else
                    {
                        current += c;
                    }
                }
                if (!current.empty())
                {
                    components.push_back(current);
                }
            }
            else
            {
                components.push_back(path);
            }
            return components;
        }

        json extractJsonValue(const json &input, const std::vector<std::string> &path)
        {
            json current = input;
            for (const auto &component : path)
            {
                bool handled = false;
                if (!component.empty() && std::all_of(component.begin(), component.end(), ::isdigit))
                {
                    size_t idx = static_cast<size_t>(std::stoull(component));
                    if (current.is_array() && idx < current.size())
                    {
                        current = current[idx];
                        handled = true;
                    }
                }
                if (handled)
                {
                    continue;
                }
                if (current.is_object() && current.contains(component))
                {
                    current = current[component];
                }
                else
                {
                    return json();
                }
            }
            return current;
        }

        bool setJsonPath(json &input, const std::vector<std::string> &path,
                         const json &new_value, std::string *error)
        {
            if (path.empty())
            {
                input = new_value;
                return true;
            }
            json *current = &input;
            for (size_t i = 0; i < path.size(); ++i)
            {
                const auto &component = path[i];
                bool last = (i + 1 == path.size());
                bool is_index = !component.empty() &&
                                std::all_of(component.begin(), component.end(), ::isdigit);
                if (is_index)
                {
                    size_t idx = static_cast<size_t>(std::stoull(component));
                    if (!current->is_array())
                    {
                        if (current->is_null())
                        {
                            *current = json::array();
                        }
                        else
                        {
                            if (error) *error = "JSON path expects array";
                            return false;
                        }
                    }
                    while (current->size() <= idx)
                    {
                        current->push_back(json());
                    }
                    if (last)
                    {
                        (*current)[idx] = new_value;
                        return true;
                    }
                    current = &(*current)[idx];
                    continue;
                }
                if (!current->is_object())
                {
                    if (current->is_null())
                    {
                        *current = json::object();
                    }
                    else
                    {
                        if (error) *error = "JSON path expects object";
                        return false;
                    }
                }
                if (last)
                {
                    (*current)[component] = new_value;
                    return true;
                }
                current = &(*current)[component];
            }
            return true;
        }

        bool computeDigest(const std::vector<uint8_t> &data,
                           const std::string &algorithm,
                           std::vector<uint8_t> &out,
                           std::string *error)
        {
            std::string algo = algorithm;
            std::transform(algo.begin(), algo.end(), algo.begin(), ::tolower);
            if (algo == "md5")
            {
                out.resize(MD5_DIGEST_LENGTH);
                MD5(data.data(), data.size(), out.data());
                return true;
            }
            if (algo == "sha1")
            {
                out.resize(SHA_DIGEST_LENGTH);
                SHA1(data.data(), data.size(), out.data());
                return true;
            }
            if (algo == "sha224")
            {
                out.resize(SHA224_DIGEST_LENGTH);
                SHA224(data.data(), data.size(), out.data());
                return true;
            }
            if (algo == "sha256")
            {
                out.resize(SHA256_DIGEST_LENGTH);
                SHA256(data.data(), data.size(), out.data());
                return true;
            }
            if (algo == "sha384")
            {
                out.resize(SHA384_DIGEST_LENGTH);
                SHA384(data.data(), data.size(), out.data());
                return true;
            }
            if (algo == "sha512")
            {
                out.resize(SHA512_DIGEST_LENGTH);
                SHA512(data.data(), data.size(), out.data());
                return true;
            }
            if (error) *error = "Unsupported digest algorithm";
            return false;
        }

        std::string normalizeIdentifier(std::string_view input)
        {
            std::string out(input);
            std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
                return static_cast<char>(std::toupper(c));
            });
            return out;
        }

        bool toBool(const core::TypedValue &value, bool &out, std::string *error)
        {
            if (value.isNull())
            {
                if (error) *error = "Boolean value cannot be NULL";
                return false;
            }
            if (value.type() == core::DataType::BOOLEAN)
            {
                out = value.getBoolean();
                return true;
            }
            out = value.toInt64() != 0;
            return true;
        }

        bool castValue(const core::TypedValue &value, const core::TypeInfo &target,
                       core::TypedValue &out, std::string *error)
        {
            if (value.isNull())
            {
                out = core::TypedValue::makeNull(target.type);
                return true;
            }
            core::ErrorContext ctx;
            core::Status status = value.convertTo(target, out, core::CastFormat::DEFAULT, &ctx);
            if (status != core::Status::OK)
            {
                if (error)
                {
                    *error = ctx.message.empty() ? "Type conversion failed" : ctx.message;
                }
                return false;
            }
            return true;
        }

        core::TypeInfo simpleTypeInfo(core::DataType type)
        {
            core::TypeInfo info(type);
            return info;
        }

        bool buildTemporalFromParts(core::DataType type,
                                    int32_t year, int32_t month, int32_t day,
                                    int32_t hour, int32_t minute, int32_t second,
                                    int32_t micro,
                                    int32_t offset_seconds,
                                    core::TypedValue &out,
                                    std::string *error)
        {
            if (type == core::DataType::DATE)
            {
                if (!isValidDate(year, month, day))
                {
                    if (error) *error = "Invalid DATE component";
                    return false;
                }
                int64_t utc_days = utcDaysFromLocalDate(year, month, day, offset_seconds);
                out = core::TypedValue::makeDate(utc_days, offset_seconds);
                return true;
            }

            if (type == core::DataType::TIME)
            {
                if (hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
                    second < 0 || second > 59 || micro < 0 || micro > 999999)
                {
                    if (error) *error = "Invalid TIME component";
                    return false;
                }
                int64_t local_micros = (static_cast<int64_t>(hour) * 3600 +
                                        static_cast<int64_t>(minute) * 60 +
                                        static_cast<int64_t>(second)) * kMicrosPerSecond +
                                       micro;
                int64_t utc_micros = local_micros -
                                     static_cast<int64_t>(offset_seconds) * kMicrosPerSecond;
                utc_micros = normalizeTimeOfDay(utc_micros);
                out = core::TypedValue::makeTime(utc_micros, offset_seconds);
                return true;
            }

            if (type == core::DataType::TIMESTAMP)
            {
                if (!isValidDate(year, month, day))
                {
                    if (error) *error = "Invalid TIMESTAMP date";
                    return false;
                }
                if (hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
                    second < 0 || second > 59 || micro < 0 || micro > 999999)
                {
                    if (error) *error = "Invalid TIMESTAMP time";
                    return false;
                }
                int64_t days = core::TypeExtractor::ymdToDays(year, month, day);
                int64_t local_micros = (days * kSecondsPerDay +
                                        (hour * 3600 + minute * 60 + second)) *
                                           kMicrosPerSecond +
                                       micro;
                int64_t utc_micros = local_micros -
                                     static_cast<int64_t>(offset_seconds) * kMicrosPerSecond;
                out = core::TypedValue::makeTimestamp(utc_micros, offset_seconds);
                return true;
            }
            if (error) *error = "Unsupported temporal type";
            return false;
        }

        void collectTsquery(const core::TSQueryNode *node,
                            std::vector<std::string> &terms,
                            std::vector<std::string> &ops,
                            int32_t &count)
        {
            if (!node)
            {
                return;
            }
            count++;
            switch (node->type())
            {
                case core::TSQueryNode::Type::LEXEME:
                    terms.push_back(node->term());
                    break;
                case core::TSQueryNode::Type::AND:
                    ops.push_back("AND");
                    collectTsquery(node->left(), terms, ops, count);
                    collectTsquery(node->right(), terms, ops, count);
                    break;
                case core::TSQueryNode::Type::OR:
                    ops.push_back("OR");
                    collectTsquery(node->left(), terms, ops, count);
                    collectTsquery(node->right(), terms, ops, count);
                    break;
                case core::TSQueryNode::Type::NOT:
                    ops.push_back("NOT");
                    collectTsquery(node->left(), terms, ops, count);
                    break;
                case core::TSQueryNode::Type::PHRASE:
                    ops.push_back("PHRASE");
                    collectTsquery(node->left(), terms, ops, count);
                    collectTsquery(node->right(), terms, ops, count);
                    break;
            }
        }

        bool isReadOnlyField(ExtractField field)
        {
            switch (field)
            {
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
                case ExtractField::HOUR12:
                case ExtractField::SIGN:
                case ExtractField::ABS:
                case ExtractField::BYTES:
                case ExtractField::BITS:
                case ExtractField::HI64:
                case ExtractField::LO64:
                case ExtractField::EXPONENT:
                case ExtractField::MANTISSA:
                case ExtractField::IS_NAN:
                case ExtractField::IS_INF:
                case ExtractField::PRECISION:
                case ExtractField::SCALE:
                case ExtractField::MAJOR:
                case ExtractField::MINOR:
                case ExtractField::CHAR_LENGTH:
                case ExtractField::OCTET_LENGTH:
                case ExtractField::CODEPOINT_LENGTH:
                case ExtractField::TRIMMED_LENGTH:
                case ExtractField::TYPE:
                case ExtractField::KEYS:
                case ExtractField::ATTRIBUTES:
                case ExtractField::LENGTH:
                case ExtractField::DIGEST:
                case ExtractField::DIMENSION:
                case ExtractField::NORM_L2:
                case ExtractField::DOT:
                case ExtractField::FIELD_NAMES:
                case ExtractField::LEXEMES:
                case ExtractField::POSITIONS:
                case ExtractField::WEIGHTS:
                case ExtractField::SIZE:
                case ExtractField::HAS_LEXEME:
                case ExtractField::ROOT_OP:
                case ExtractField::TERMS:
                case ExtractField::OPERATORS:
                case ExtractField::PHRASE_DISTANCE:
                case ExtractField::NODES:
                case ExtractField::VERSION:
                case ExtractField::VARIANT:
                case ExtractField::TIMESTAMP:
                case ExtractField::CLOCK_SEQ:
                case ExtractField::NODE:
                case ExtractField::TIME_LOW:
                case ExtractField::TIME_MID:
                case ExtractField::TIME_HIGH:
                case ExtractField::RAND_A:
                case ExtractField::RAND_B:
                case ExtractField::NETWORK:
                case ExtractField::BROADCAST:
                case ExtractField::NETMASK_ADDR:
                case ExtractField::HOSTMASK:
                case ExtractField::IS_IPV4:
                case ExtractField::IS_IPV6:
                case ExtractField::IS_MULTICAST:
                case ExtractField::IS_LOCAL:
                case ExtractField::TRUNC:
                case ExtractField::NUM_POINTS:
                case ExtractField::START_POINT:
                case ExtractField::END_POINT:
                case ExtractField::NUM_RINGS:
                case ExtractField::EXTERIOR_RING:
                case ExtractField::NUM_INTERIOR_RINGS:
                case ExtractField::NUM_GEOMETRIES:
                case ExtractField::BBOX:
                case ExtractField::POINTS:
                case ExtractField::RINGS:
                case ExtractField::AREA:
                case ExtractField::GEOMETRIES:
                    return true;
                default:
                    return false;
            }
        }
    } // namespace

    bool extractElement(const core::TypedValue& source,
                        ExtractField field,
                        const std::vector<core::TypedValue>& args,
                        core::TypedValue* out,
                        std::string* error)
    {
        if (!out)
        {
            if (error) *error = "Missing output";
            return false;
        }
        if (source.isNull())
        {
            *out = core::TypedValue::makeNull();
            return true;
        }

        ElementArgSpec arg_spec = extractFieldArgSpec(field);
        if (args.size() < arg_spec.min_args || args.size() > arg_spec.max_args)
        {
            if (error)
            {
                std::ostringstream oss;
                oss << "Invalid argument count for EXTRACT(" << extractFieldToString(field)
                    << ")";
                *error = oss.str();
            }
            return false;
        }

        core::DataType source_type = source.type();

        if (source_type == core::DataType::UNKNOWN || source_type == core::DataType::NULL_TYPE)
        {
            if (field == ExtractField::VALUE)
            {
                *out = source;
                return true;
            }
            if (error) *error = "EXTRACT not supported for UNKNOWN type";
            return false;
        }

        if (source_type == core::DataType::DATE)
        {
            int32_t offset_seconds = source.getTimezoneOffsetSeconds();
            int64_t local_days = localDaysFromDate(source.getDate(), offset_seconds);
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::YEAR:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractYear(local_days));
                    return true;
                case ExtractField::MONTH:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractMonth(local_days));
                    return true;
                case ExtractField::DAY:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractDay(local_days));
                    return true;
                case ExtractField::DOW:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractDayOfWeek(local_days));
                    return true;
                case ExtractField::DOY:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractDayOfYear(local_days));
                    return true;
                case ExtractField::QUARTER:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractQuarter(local_days));
                    return true;
                case ExtractField::WEEK:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractWeek(local_days));
                    return true;
                case ExtractField::ISO_WEEK:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractISOWeek(local_days));
                    return true;
                case ExtractField::ISO_YEAR:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractISOYear(local_days));
                    return true;
                case ExtractField::ISO_DOW:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractISODayOfWeek(local_days));
                    return true;
                case ExtractField::CENTURY:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractCentury(local_days));
                    return true;
                case ExtractField::DECADE:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractDecade(local_days));
                    return true;
                case ExtractField::MILLENNIUM:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractMillennium(local_days));
                    return true;
                case ExtractField::EPOCH:
                    *out = core::TypedValue::makeInt64(local_days * kSecondsPerDay);
                    return true;
                case ExtractField::TIMEZONE:
                case ExtractField::TZ_OFFSET:
                    *out = core::TypedValue::makeInt32(offset_seconds);
                    return true;
                case ExtractField::TIMEZONE_HOUR:
                {
                    int32_t total_minutes = offset_seconds / 60;
                    *out = core::TypedValue::makeInt32(total_minutes / 60);
                    return true;
                }
                case ExtractField::TIMEZONE_MINUTE:
                {
                    int32_t total_minutes = offset_seconds / 60;
                    int32_t minute = std::abs(total_minutes % 60);
                    if (offset_seconds < 0)
                    {
                        minute = -minute;
                    }
                    *out = core::TypedValue::makeInt32(minute);
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::TIME)
        {
            int32_t offset_seconds = source.getTimezoneOffsetSeconds();
            int64_t local_micros = normalizeTimeOfDay(source.getTime() +
                                                      static_cast<int64_t>(offset_seconds) *
                                                          kMicrosPerSecond);
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::HOUR:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractHour(local_micros));
                    return true;
                case ExtractField::HOUR12:
                {
                    int32_t hour = core::TypeExtractor::extractHour(local_micros);
                    int32_t hour12 = hour % 12;
                    if (hour12 == 0)
                    {
                        hour12 = 12;
                    }
                    *out = core::TypedValue::makeInt32(hour12);
                    return true;
                }
                case ExtractField::MINUTE:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractMinute(local_micros));
                    return true;
                case ExtractField::SECOND:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractSecond(local_micros));
                    return true;
                case ExtractField::MICROSECOND:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractMicrosecond(local_micros));
                    return true;
                case ExtractField::MILLISECOND:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractMillisecond(local_micros));
                    return true;
                case ExtractField::EPOCH:
                    *out = core::TypedValue::makeInt64(local_micros / kMicrosPerSecond);
                    return true;
                case ExtractField::TIMEZONE:
                case ExtractField::TZ_OFFSET:
                    *out = core::TypedValue::makeInt32(offset_seconds);
                    return true;
                case ExtractField::TIMEZONE_HOUR:
                {
                    int32_t total_minutes = offset_seconds / 60;
                    *out = core::TypedValue::makeInt32(total_minutes / 60);
                    return true;
                }
                case ExtractField::TIMEZONE_MINUTE:
                {
                    int32_t total_minutes = offset_seconds / 60;
                    int32_t minute = std::abs(total_minutes % 60);
                    if (offset_seconds < 0)
                    {
                        minute = -minute;
                    }
                    *out = core::TypedValue::makeInt32(minute);
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::TIMESTAMP)
        {
            int32_t offset_seconds = source.getTimezoneOffsetSeconds();
            int64_t local_micros = source.getTimestamp() +
                                   static_cast<int64_t>(offset_seconds) * kMicrosPerSecond;
            int64_t local_seconds = floorDiv(local_micros, kMicrosPerSecond);
            int64_t local_days = floorDiv(local_seconds, kSecondsPerDay);
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::YEAR:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractYear(local_days));
                    return true;
                case ExtractField::MONTH:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractMonth(local_days));
                    return true;
                case ExtractField::DAY:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractDay(local_days));
                    return true;
                case ExtractField::HOUR:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractHour(local_micros));
                    return true;
                case ExtractField::MINUTE:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractMinute(local_micros));
                    return true;
                case ExtractField::SECOND:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractSecond(local_micros));
                    return true;
                case ExtractField::MICROSECOND:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractMicrosecond(local_micros));
                    return true;
                case ExtractField::MILLISECOND:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractMillisecond(local_micros));
                    return true;
                case ExtractField::DOW:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractDayOfWeek(local_days));
                    return true;
                case ExtractField::DOY:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractDayOfYear(local_days));
                    return true;
                case ExtractField::QUARTER:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractQuarter(local_days));
                    return true;
                case ExtractField::WEEK:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractWeek(local_days));
                    return true;
                case ExtractField::ISO_WEEK:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractISOWeek(local_days));
                    return true;
                case ExtractField::ISO_YEAR:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractISOYear(local_days));
                    return true;
                case ExtractField::ISO_DOW:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractISODayOfWeek(local_days));
                    return true;
                case ExtractField::CENTURY:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractCentury(local_days));
                    return true;
                case ExtractField::DECADE:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractDecade(local_days));
                    return true;
                case ExtractField::MILLENNIUM:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractMillennium(local_days));
                    return true;
                case ExtractField::EPOCH:
                    *out = core::TypedValue::makeInt64(local_micros / kMicrosPerSecond);
                    return true;
                case ExtractField::TIMEZONE:
                case ExtractField::TZ_OFFSET:
                    *out = core::TypedValue::makeInt32(offset_seconds);
                    return true;
                case ExtractField::TIMEZONE_HOUR:
                {
                    int32_t total_minutes = offset_seconds / 60;
                    *out = core::TypedValue::makeInt32(total_minutes / 60);
                    return true;
                }
                case ExtractField::TIMEZONE_MINUTE:
                {
                    int32_t total_minutes = offset_seconds / 60;
                    int32_t minute = std::abs(total_minutes % 60);
                    if (offset_seconds < 0)
                    {
                        minute = -minute;
                    }
                    *out = core::TypedValue::makeInt32(minute);
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::INTERVAL)
        {
            core::Interval interval = source.getInterval();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::YEAR:
                    *out = core::TypedValue::makeInt32(interval.months / 12);
                    return true;
                case ExtractField::MONTH:
                    *out = core::TypedValue::makeInt32(interval.months % 12);
                    return true;
                case ExtractField::DAY:
                    *out = core::TypedValue::makeInt32(interval.days);
                    return true;
                case ExtractField::HOUR:
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(interval.microseconds /
                                                                              (3600 * kMicrosPerSecond)));
                    return true;
                case ExtractField::MINUTE:
                {
                    int64_t total_minutes = interval.microseconds / (60 * kMicrosPerSecond);
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(total_minutes % 60));
                    return true;
                }
                case ExtractField::SECOND:
                {
                    int64_t total_seconds = interval.microseconds / kMicrosPerSecond;
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(total_seconds % 60));
                    return true;
                }
                case ExtractField::MILLISECOND:
                {
                    int64_t ms = (interval.microseconds / 1000) % 1000;
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(ms));
                    return true;
                }
                case ExtractField::MICROSECOND:
                {
                    int64_t us = interval.microseconds % kMicrosPerSecond;
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(us));
                    return true;
                }
                case ExtractField::TOTAL_MONTHS:
                    *out = core::TypedValue::makeInt32(interval.months);
                    return true;
                case ExtractField::TOTAL_DAYS:
                    *out = core::TypedValue::makeInt64(interval.days);
                    return true;
                case ExtractField::TOTAL_SECONDS:
                    *out = core::TypedValue::makeFloat64(interval.microseconds /
                                                         static_cast<double>(kMicrosPerSecond));
                    return true;
                case ExtractField::EPOCH:
                {
                    double total_seconds = (interval.months * 30.0 * kSecondsPerDay) +
                                           (interval.days * kSecondsPerDay) +
                                           (interval.microseconds /
                                            static_cast<double>(kMicrosPerSecond));
                    *out = core::TypedValue::makeFloat64(total_seconds);
                    return true;
                }
                case ExtractField::SIGN:
                {
                    int sign = 0;
                    if (interval.months != 0 || interval.days != 0 || interval.microseconds != 0)
                    {
                        int64_t combined = static_cast<int64_t>(interval.months) +
                                           static_cast<int64_t>(interval.days) +
                                           (interval.microseconds != 0 ? (interval.microseconds > 0 ? 1 : -1) : 0);
                        sign = combined > 0 ? 1 : -1;
                    }
                    *out = core::TypedValue::makeInt8(static_cast<int8_t>(sign));
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::UUID)
        {
            const std::vector<uint8_t> uuid = source.getUUID();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::BYTES:
                    *out = core::TypedValue::makeBinary(uuid);
                    return true;
                case ExtractField::VERSION:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractUUIDVersion(uuid));
                    return true;
                case ExtractField::VARIANT:
                    *out = core::TypedValue::makeInt32(core::TypeExtractor::extractUUIDVariant(uuid));
                    return true;
                case ExtractField::TIMESTAMP:
                {
                    auto timestamp = core::TypeExtractor::extractUUIDTimestamp(uuid, nullptr);
                    if (!timestamp.has_value())
                    {
                        if (error) *error = "UUID timestamp extraction failed";
                        return false;
                    }
                    *out = core::TypedValue::makeInt64(*timestamp);
                    return true;
                }
                case ExtractField::CLOCK_SEQ:
                {
                    int version = core::TypeExtractor::extractUUIDVersion(uuid);
                    if (version != 1 || uuid.size() < 10)
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    int32_t clock_seq = ((uuid[8] & 0x3F) << 8) | uuid[9];
                    *out = core::TypedValue::makeInt32(clock_seq);
                    return true;
                }
                case ExtractField::NODE:
                {
                    int version = core::TypeExtractor::extractUUIDVersion(uuid);
                    if (version != 1 || uuid.size() < 16)
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    static const char hex_chars[] = "0123456789abcdef";
                    std::string mac;
                    mac.reserve(17);
                    for (size_t i = 10; i < 16; i++)
                    {
                        if (i > 10) mac.push_back(':');
                        mac.push_back(hex_chars[(uuid[i] >> 4) & 0x0F]);
                        mac.push_back(hex_chars[uuid[i] & 0x0F]);
                    }
                    *out = core::TypedValue::makeVarchar(mac);
                    return true;
                }
                case ExtractField::TIME_LOW:
                {
                    if (core::TypeExtractor::extractUUIDVersion(uuid) != 1 || uuid.size() < 4)
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    uint32_t time_low = (uint32_t(uuid[0]) << 24) | (uint32_t(uuid[1]) << 16) |
                                        (uint32_t(uuid[2]) << 8) | uint32_t(uuid[3]);
                    *out = core::TypedValue::makeUInt32(time_low);
                    return true;
                }
                case ExtractField::TIME_MID:
                {
                    if (core::TypeExtractor::extractUUIDVersion(uuid) != 1 || uuid.size() < 6)
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    uint16_t time_mid = (uint16_t(uuid[4]) << 8) | uint16_t(uuid[5]);
                    *out = core::TypedValue::makeUInt16(time_mid);
                    return true;
                }
                case ExtractField::TIME_HIGH:
                {
                    if (core::TypeExtractor::extractUUIDVersion(uuid) != 1 || uuid.size() < 8)
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    uint16_t time_high = (uint16_t(uuid[6] & 0x0F) << 8) | uint16_t(uuid[7]);
                    *out = core::TypedValue::makeUInt16(time_high);
                    return true;
                }
                case ExtractField::RAND_A:
                {
                    if (core::TypeExtractor::extractUUIDVersion(uuid) != 7 || uuid.size() < 8)
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    uint32_t rand_a = (uint32_t(uuid[6] & 0x0F) << 8) | uint32_t(uuid[7]);
                    *out = core::TypedValue::makeUInt32(rand_a);
                    return true;
                }
                case ExtractField::RAND_B:
                {
                    if (core::TypeExtractor::extractUUIDVersion(uuid) != 7 || uuid.size() < 16)
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    std::vector<uint8_t> bytes(uuid.begin() + 8, uuid.begin() + 16);
                    *out = core::TypedValue::makeBinary(bytes);
                    return true;
                }
                default:
                    break;
            }
        }
        if (source_type == core::DataType::INT8 || source_type == core::DataType::INT16 ||
            source_type == core::DataType::INT32 || source_type == core::DataType::INT64 ||
            source_type == core::DataType::INT128 || source_type == core::DataType::UINT8 ||
            source_type == core::DataType::UINT16 || source_type == core::DataType::UINT32 ||
            source_type == core::DataType::UINT64 || source_type == core::DataType::UINT128)
        {
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::SIGN:
                {
                    int sign = 0;
                    if (source_type == core::DataType::INT128)
                    {
                        core::int128_t val = source.getInt128();
                        sign = (val > 0) - (val < 0);
                    }
                    else if (source_type == core::DataType::UINT128)
                    {
                        core::uint128_t val = source.getUInt128();
                        sign = val == 0 ? 0 : 1;
                    }
                    else if (source_type == core::DataType::UINT8 || source_type == core::DataType::UINT16 ||
                             source_type == core::DataType::UINT32 || source_type == core::DataType::UINT64)
                    {
                        sign = source.toInt64() == 0 ? 0 : 1;
                    }
                    else
                    {
                        int64_t val = source.toInt64();
                        sign = (val > 0) - (val < 0);
                    }
                    *out = core::TypedValue::makeInt8(static_cast<int8_t>(sign));
                    return true;
                }
                case ExtractField::ABS:
                {
                    if (source_type == core::DataType::INT128)
                    {
                        core::int128_t val = source.getInt128();
                        if (val == std::numeric_limits<core::int128_t>::min())
                        {
                            if (error) *error = "ABS overflow for INT128";
                            return false;
                        }
                        if (val < 0)
                        {
                            core::uint128_t abs = static_cast<core::uint128_t>(-val);
                            std::vector<uint8_t> bytes(16);
                            for (int i = 0; i < 16; ++i)
                            {
                                bytes[i] = static_cast<uint8_t>((abs >> (i * 8)) & 0xFF);
                            }
                            *out = core::TypedValue::makeInt128(bytes);
                        }
                        else
                        {
                            *out = source;
                        }
                        return true;
                    }
                    if (source_type == core::DataType::UINT128 || source_type == core::DataType::UINT8 ||
                        source_type == core::DataType::UINT16 || source_type == core::DataType::UINT32 ||
                        source_type == core::DataType::UINT64)
                    {
                        *out = source;
                        return true;
                    }
                    int64_t val = source.toInt64();
                    if (val == std::numeric_limits<int64_t>::min())
                    {
                        if (error) *error = "ABS overflow";
                        return false;
                    }
                    int64_t abs = val < 0 ? -val : val;
                    switch (source_type)
                    {
                        case core::DataType::INT8:
                            *out = core::TypedValue::makeInt8(static_cast<int8_t>(abs));
                            return true;
                        case core::DataType::INT16:
                            *out = core::TypedValue::makeInt16(static_cast<int16_t>(abs));
                            return true;
                        case core::DataType::INT32:
                            *out = core::TypedValue::makeInt32(static_cast<int32_t>(abs));
                            return true;
                        case core::DataType::INT64:
                            *out = core::TypedValue::makeInt64(abs);
                            return true;
                        default:
                            break;
                    }
                    break;
                }
                case ExtractField::BYTES:
                {
                    int32_t bytes = 0;
                    switch (source_type)
                    {
                        case core::DataType::INT8:
                        case core::DataType::UINT8:
                            bytes = 1; break;
                        case core::DataType::INT16:
                        case core::DataType::UINT16:
                            bytes = 2; break;
                        case core::DataType::INT32:
                        case core::DataType::UINT32:
                            bytes = 4; break;
                        case core::DataType::INT64:
                        case core::DataType::UINT64:
                            bytes = 8; break;
                        case core::DataType::INT128:
                        case core::DataType::UINT128:
                            bytes = 16; break;
                        default:
                            break;
                    }
                    *out = core::TypedValue::makeInt16(static_cast<int16_t>(bytes));
                    return true;
                }
                case ExtractField::BITS:
                {
                    int32_t bits = 0;
                    switch (source_type)
                    {
                        case core::DataType::INT8:
                        case core::DataType::UINT8:
                            bits = 8; break;
                        case core::DataType::INT16:
                        case core::DataType::UINT16:
                            bits = 16; break;
                        case core::DataType::INT32:
                        case core::DataType::UINT32:
                            bits = 32; break;
                        case core::DataType::INT64:
                        case core::DataType::UINT64:
                            bits = 64; break;
                        case core::DataType::INT128:
                        case core::DataType::UINT128:
                            bits = 128; break;
                        default:
                            break;
                    }
                    *out = core::TypedValue::makeInt16(static_cast<int16_t>(bits));
                    return true;
                }
                case ExtractField::HI64:
                case ExtractField::LO64:
                {
                    if (source_type != core::DataType::INT128 && source_type != core::DataType::UINT128)
                    {
                        if (error) *error = "HI64/LO64 only valid for 128-bit integers";
                        return false;
                    }
                    core::uint128_t val = (source_type == core::DataType::INT128)
                                               ? static_cast<core::uint128_t>(source.getInt128())
                                               : source.getUInt128();
                    uint64_t hi = static_cast<uint64_t>(val >> 64);
                    uint64_t lo = static_cast<uint64_t>(val & 0xFFFFFFFFFFFFFFFFULL);
                    *out = core::TypedValue::makeUInt64(field == ExtractField::HI64 ? hi : lo);
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::FLOAT32 || source_type == core::DataType::FLOAT64)
        {
            double val = source_type == core::DataType::FLOAT32 ? source.getFloat32() : source.getFloat64();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::SIGN:
                {
                    int sign = 0;
                    if (!std::isnan(val) && val != 0.0)
                    {
                        sign = (val > 0.0) - (val < 0.0);
                    }
                    *out = core::TypedValue::makeInt8(static_cast<int8_t>(sign));
                    return true;
                }
                case ExtractField::EXPONENT:
                {
                    int exp = 0;
                    std::frexp(val, &exp);
                    *out = core::TypedValue::makeInt32(exp);
                    return true;
                }
                case ExtractField::MANTISSA:
                {
                    int exp = 0;
                    double mant = std::frexp(val, &exp);
                    int bits = (source_type == core::DataType::FLOAT32) ? 23 : 52;
                    int64_t mantissa = static_cast<int64_t>(std::ldexp(std::fabs(mant), bits));
                    *out = core::TypedValue::makeInt64(mantissa);
                    return true;
                }
                case ExtractField::IS_NAN:
                    *out = core::TypedValue::makeBool(std::isnan(val));
                    return true;
                case ExtractField::IS_INF:
                    *out = core::TypedValue::makeBool(std::isinf(val));
                    return true;
                default:
                    break;
            }
        }

        if (source_type == core::DataType::DECIMAL ||
            source_type == core::DataType::DECFLOAT16 ||
            source_type == core::DataType::DECFLOAT34)
        {
            uint8_t precision = source.getDecimalPrecision();
            if (precision == 0)
            {
                if (source_type == core::DataType::DECFLOAT16)
                {
                    precision = 16;
                }
                else if (source_type == core::DataType::DECFLOAT34)
                {
                    precision = 34;
                }
                else
                {
                    precision = core::DECIMAL_MAX_PRECISION;
                }
            }
            uint8_t scale = source.getDecimalScale();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::PRECISION:
                    *out = core::TypedValue::makeInt16(static_cast<int16_t>(precision));
                    return true;
                case ExtractField::SCALE:
                    *out = core::TypedValue::makeInt16(static_cast<int16_t>(scale));
                    return true;
                case ExtractField::UNSCALED:
                {
                    core::int128_t unscaled = source.getDecimalUnscaled();
                    std::vector<uint8_t> bytes(16);
                    core::uint128_t uval = static_cast<core::uint128_t>(unscaled);
                    for (int i = 0; i < 16; ++i)
                    {
                        bytes[i] = static_cast<uint8_t>((uval >> (i * 8)) & 0xFF);
                    }
                    *out = core::TypedValue::makeInt128(bytes);
                    return true;
                }
                case ExtractField::SIGN:
                {
                    core::int128_t unscaled = source.getDecimalUnscaled();
                    int sign = (unscaled > 0) - (unscaled < 0);
                    *out = core::TypedValue::makeInt8(static_cast<int8_t>(sign));
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::MONEY)
        {
            int64_t val = source.toInt64();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::SCALE:
                    *out = core::TypedValue::makeInt8(4);
                    return true;
                case ExtractField::MAJOR:
                    *out = core::TypedValue::makeInt64(val / 10000);
                    return true;
                case ExtractField::MINOR:
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(std::llabs(val) % 10000));
                    return true;
                case ExtractField::SIGN:
                {
                    int sign = (val > 0) - (val < 0);
                    *out = core::TypedValue::makeInt8(static_cast<int8_t>(sign));
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::BOOLEAN)
        {
            if (field == ExtractField::VALUE)
            {
                *out = source;
                return true;
            }
        }
        if (source_type == core::DataType::CHAR || source_type == core::DataType::VARCHAR ||
            source_type == core::DataType::TEXT)
        {
            const std::string str = source.toString();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::CHAR_LENGTH:
                case ExtractField::CODEPOINT_LENGTH:
                {
                    size_t len = core::UTF8Utils::countCharacters(str);
                    if (len == 0 && !str.empty())
                    {
                        len = str.size();
                    }
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(len));
                    return true;
                }
                case ExtractField::OCTET_LENGTH:
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(str.size()));
                    return true;
                case ExtractField::TRIMMED_LENGTH:
                {
                    if (source_type != core::DataType::CHAR)
                    {
                        if (error) *error = "TRIMMED_LENGTH only valid for CHAR";
                        return false;
                    }
                    std::string trimmed = str;
                    while (!trimmed.empty() && trimmed.back() == ' ')
                    {
                        trimmed.pop_back();
                    }
                    size_t len = core::UTF8Utils::countCharacters(trimmed);
                    if (len == 0 && !trimmed.empty())
                    {
                        len = trimmed.size();
                    }
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(len));
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::JSON || source_type == core::DataType::JSONB)
        {
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::PATH:
                {
                    std::string path;
                    if (!parseStringArg(args, 0, path, error))
                    {
                        return false;
                    }
                    json doc;
                    if (!parseJson(source, doc, error))
                    {
                        return false;
                    }
                    std::vector<std::string> components = parseJsonPath(path);
                    json result = extractJsonValue(doc, components);
                    if (result.is_discarded() || result.is_null())
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    *out = jsonToValue(result, source_type);
                    return true;
                }
                case ExtractField::TYPE:
                {
                    json doc;
                    if (!parseJson(source, doc, error))
                    {
                        return false;
                    }
                    std::string type_str = "null";
                    if (doc.is_object()) type_str = "object";
                    else if (doc.is_array()) type_str = "array";
                    else if (doc.is_string()) type_str = "string";
                    else if (doc.is_number()) type_str = "number";
                    else if (doc.is_boolean()) type_str = "boolean";
                    *out = core::TypedValue::makeText(type_str);
                    return true;
                }
                case ExtractField::KEYS:
                {
                    json doc;
                    if (!parseJson(source, doc, error))
                    {
                        return false;
                    }
                    if (!doc.is_object())
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    std::vector<core::TypedValue> keys;
                    keys.reserve(doc.size());
                    for (auto it = doc.begin(); it != doc.end(); ++it)
                    {
                        keys.push_back(core::TypedValue::makeText(it.key()));
                    }
                    *out = core::TypedValue::makeArray(keys);
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::XML)
        {
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::PATH:
                case ExtractField::ATTRIBUTES:
                {
#ifdef HAVE_LIBXML2
                    std::string xml_text = source.toString();
                    std::string xpath;
                    if (field == ExtractField::PATH)
                    {
                        if (!parseStringArg(args, 0, xpath, error))
                        {
                            return false;
                        }
                    }
                    xmlDocPtr doc = xmlReadMemory(xml_text.c_str(),
                                                  static_cast<int>(xml_text.size()),
                                                  nullptr, nullptr,
                                                  XML_PARSE_NONET | XML_PARSE_NOENT);
                    if (!doc)
                    {
                        if (error) *error = "Invalid XML";
                        return false;
                    }

                    if (field == ExtractField::ATTRIBUTES)
                    {
                        xmlNodePtr root = xmlDocGetRootElement(doc);
                        std::vector<core::TypedValue> attrs;
                        if (root)
                        {
                            for (xmlAttrPtr attr = root->properties; attr; attr = attr->next)
                            {
                                attrs.push_back(core::TypedValue::makeText(reinterpret_cast<const char*>(attr->name)));
                            }
                        }
                        xmlFreeDoc(doc);
                        *out = core::TypedValue::makeArray(attrs);
                        return true;
                    }

                    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
                    if (!xpathCtx)
                    {
                        xmlFreeDoc(doc);
                        if (error) *error = "XML XPath context error";
                        return false;
                    }
                    xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(
                        reinterpret_cast<const xmlChar*>(xpath.c_str()),
                        xpathCtx);
                    if (!xpathObj)
                    {
                        xmlXPathFreeContext(xpathCtx);
                        xmlFreeDoc(doc);
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    if (xpathObj->type != XPATH_NODESET || !xpathObj->nodesetval ||
                        xpathObj->nodesetval->nodeNr == 0)
                    {
                        xmlXPathFreeObject(xpathObj);
                        xmlXPathFreeContext(xpathCtx);
                        xmlFreeDoc(doc);
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    xmlNodePtr node = xpathObj->nodesetval->nodeTab[0];
                    xmlBufferPtr buffer = xmlBufferCreate();
                    xmlNodeDump(buffer, doc, node, 0, 1);
                    std::string result(reinterpret_cast<const char*>(buffer->content));
                    xmlBufferFree(buffer);
                    xmlXPathFreeObject(xpathObj);
                    xmlXPathFreeContext(xpathCtx);
                    xmlFreeDoc(doc);
                    *out = core::TypedValue::makeXML(result);
                    return true;
#else
                    if (error) *error = "XML support not available";
                    return false;
#endif
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::BINARY || source_type == core::DataType::VARBINARY ||
            source_type == core::DataType::BLOB || source_type == core::DataType::BYTEA)
        {
            std::vector<uint8_t> data = source.getBinary();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::LENGTH:
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(data.size()));
                    return true;
                case ExtractField::BYTE:
                {
                    int64_t index = 0;
                    if (!parseIntegerArg(args, 0, index, error))
                    {
                        return false;
                    }
                    if (index < 0 || static_cast<size_t>(index) >= data.size())
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    *out = core::TypedValue::makeUInt8(data[static_cast<size_t>(index)]);
                    return true;
                }
                case ExtractField::BIT:
                {
                    int64_t index = 0;
                    if (!parseIntegerArg(args, 0, index, error))
                    {
                        return false;
                    }
                    if (index < 0 || static_cast<size_t>(index / 8) >= data.size())
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    size_t byte_index = static_cast<size_t>(index / 8);
                    uint8_t byte = data[byte_index];
                    int bit_index = static_cast<int>(index % 8);
                    uint8_t bit = (byte >> (7 - bit_index)) & 0x01;
                    *out = core::TypedValue::makeUInt8(bit);
                    return true;
                }
                case ExtractField::SLICE:
                {
                    int64_t start = 0;
                    int64_t length = 0;
                    if (!parseIntegerArg(args, 0, start, error) ||
                        !parseIntegerArg(args, 1, length, error))
                    {
                        return false;
                    }
                    if (start < 0 || length < 0)
                    {
                        if (error) *error = "Invalid slice bounds";
                        return false;
                    }
                    if (static_cast<size_t>(start) >= data.size())
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    size_t end = std::min(static_cast<size_t>(start + length), data.size());
                    std::vector<uint8_t> slice(data.begin() + start, data.begin() + end);
                    if (source_type == core::DataType::BLOB)
                    {
                        *out = core::TypedValue::makeBlob(slice);
                    }
                    else if (source_type == core::DataType::BYTEA)
                    {
                        *out = core::TypedValue::makeBytea(slice);
                    }
                    else if (source_type == core::DataType::VARBINARY)
                    {
                        *out = core::TypedValue::makeVarbinary(slice);
                    }
                    else
                    {
                        *out = core::TypedValue::makeBinary(slice);
                    }
                    return true;
                }
                case ExtractField::DIGEST:
                {
                    std::string algorithm;
                    if (!parseStringArg(args, 0, algorithm, error))
                    {
                        return false;
                    }
                    std::vector<uint8_t> digest;
                    if (!computeDigest(data, algorithm, digest, error))
                    {
                        return false;
                    }
                    *out = core::TypedValue::makeBinary(digest);
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::VECTOR)
        {
            std::vector<float> vec;
            if (!decodeVectorValue(source, vec, error))
            {
                return false;
            }
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::DIMENSION:
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(vec.size()));
                    return true;
                case ExtractField::ELEMENT:
                {
                    int64_t index = 0;
                    if (!parseIntegerArg(args, 0, index, error))
                    {
                        return false;
                    }
                    if (index <= 0 || static_cast<size_t>(index) > vec.size())
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    *out = core::TypedValue::makeFloat32(vec[static_cast<size_t>(index - 1)]);
                    return true;
                }
                case ExtractField::NORM_L2:
                {
                    double sum = 0.0;
                    for (float v : vec)
                    {
                        sum += static_cast<double>(v) * static_cast<double>(v);
                    }
                    *out = core::TypedValue::makeFloat64(std::sqrt(sum));
                    return true;
                }
                case ExtractField::DOT:
                {
                    if (args.size() != 1)
                    {
                        if (error) *error = "DOT expects one argument";
                        return false;
                    }
                    std::vector<float> other;
                    if (!decodeVectorValue(args[0], other, error))
                    {
                        return false;
                    }
                    if (other.size() != vec.size())
                    {
                        if (error) *error = "Vector dimension mismatch";
                        return false;
                    }
                    double sum = 0.0;
                    for (size_t i = 0; i < vec.size(); ++i)
                    {
                        sum += static_cast<double>(vec[i]) * static_cast<double>(other[i]);
                    }
                    *out = core::TypedValue::makeFloat64(sum);
                    return true;
                }
                default:
                    break;
            }
        }
        if (source_type == core::DataType::ARRAY)
        {
            const auto &array = source.getArray();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::ELEMENT:
                {
                    int64_t index = 0;
                    if (!parseIntegerArg(args, 0, index, error))
                    {
                        return false;
                    }
                    if (index <= 0 || static_cast<size_t>(index) > array.size())
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    *out = array[static_cast<size_t>(index - 1)];
                    return true;
                }
                case ExtractField::CARDINALITY:
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(array.size()));
                    return true;
                case ExtractField::NDIMS:
                    *out = core::TypedValue::makeInt32(1);
                    return true;
                case ExtractField::LOWER:
                {
                    int64_t dim = 1;
                    if (!args.empty())
                    {
                        if (!parseIntegerArg(args, 0, dim, error))
                        {
                            return false;
                        }
                    }
                    if (dim != 1)
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    *out = core::TypedValue::makeInt32(1);
                    return true;
                }
                case ExtractField::UPPER:
                {
                    int64_t dim = 1;
                    if (!args.empty())
                    {
                        if (!parseIntegerArg(args, 0, dim, error))
                        {
                            return false;
                        }
                    }
                    if (dim != 1)
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(array.size()));
                    return true;
                }
                case ExtractField::DIMS:
                {
                    std::vector<core::TypedValue> dims;
                    dims.push_back(core::TypedValue::makeInt32(static_cast<int32_t>(array.size())));
                    *out = core::TypedValue::makeArray(dims);
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::COMPOSITE)
        {
            const auto &values = source.getCompositeValues();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::FIELD:
                {
                    if (args.empty())
                    {
                        if (error) *error = "FIELD requires a selector";
                        return false;
                    }
                    if (!args[0].isNull() && (args[0].type() == core::DataType::INT32 ||
                                              args[0].type() == core::DataType::INT64 ||
                                              args[0].type() == core::DataType::INT16 ||
                                              args[0].type() == core::DataType::INT8))
                    {
                        int64_t index = args[0].toInt64();
                        if (index <= 0 || static_cast<size_t>(index) > values.size())
                        {
                            *out = core::TypedValue::makeNull();
                            return true;
                        }
                        *out = values[static_cast<size_t>(index - 1)];
                        return true;
                    }
                    std::string name = args[0].toString();
                    std::vector<std::string> names = source.getCompositeFieldNames();
                    std::string normalized = normalizeIdentifier(name);
                    for (size_t i = 0; i < names.size(); ++i)
                    {
                        if (normalizeIdentifier(names[i]) == normalized)
                        {
                            *out = values[i];
                            return true;
                        }
                    }
                    *out = core::TypedValue::makeNull();
                    return true;
                }
                case ExtractField::FIELD_NAMES:
                {
                    std::vector<std::string> names = source.getCompositeFieldNames();
                    std::vector<core::TypedValue> out_names;
                    out_names.reserve(names.size());
                    for (const auto &name : names)
                    {
                        out_names.push_back(core::TypedValue::makeText(name));
                    }
                    *out = core::TypedValue::makeArray(out_names);
                    return true;
                }
                case ExtractField::CARDINALITY:
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(values.size()));
                    return true;
                default:
                    break;
            }
        }

        if (source_type == core::DataType::VARIANT)
        {
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source.getVariantValue();
                    return true;
                case ExtractField::DATATYPE:
                {
                    auto tag = source.getVariantTag();
                    core::DataType dtype = tag.has_value() ? *tag : source.getVariantValue().type();
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(dtype));
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::TSVECTOR)
        {
            const auto &tsvector = source.getTSVector();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::LEXEMES:
                {
                    std::vector<core::TypedValue> lexemes;
                    lexemes.reserve(tsvector.lexemes().size());
                    for (const auto &lex : tsvector.lexemes())
                    {
                        lexemes.push_back(core::TypedValue::makeText(lex.word));
                    }
                    *out = core::TypedValue::makeArray(lexemes);
                    return true;
                }
                case ExtractField::POSITIONS:
                {
                    std::vector<core::TypedValue> outer;
                    outer.reserve(tsvector.lexemes().size());
                    for (const auto &lex : tsvector.lexemes())
                    {
                        std::vector<core::TypedValue> positions;
                        positions.reserve(lex.positions.size());
                        for (uint16_t pos : lex.positions)
                        {
                            positions.push_back(core::TypedValue::makeInt32(static_cast<int32_t>(pos)));
                        }
                        outer.push_back(core::TypedValue::makeArray(positions));
                    }
                    *out = core::TypedValue::makeArray(outer);
                    return true;
                }
                case ExtractField::WEIGHTS:
                {
                    std::vector<core::TypedValue> outer;
                    outer.reserve(tsvector.lexemes().size());
                    for (const auto &lex : tsvector.lexemes())
                    {
                        std::vector<core::TypedValue> weights;
                        weights.reserve(lex.positions.size());
                        for (size_t i = 0; i < lex.positions.size(); ++i)
                        {
                            std::string w(1, lex.getWeight(i));
                            weights.push_back(core::TypedValue::makeText(w));
                        }
                        outer.push_back(core::TypedValue::makeArray(weights));
                    }
                    *out = core::TypedValue::makeArray(outer);
                    return true;
                }
                case ExtractField::SIZE:
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(tsvector.numLexemes()));
                    return true;
                case ExtractField::HAS_LEXEME:
                {
                    std::string term;
                    if (!parseStringArg(args, 0, term, error))
                    {
                        return false;
                    }
                    *out = core::TypedValue::makeBool(tsvector.contains(term));
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::TSQUERY)
        {
            const auto &tsquery = source.getTSQuery();
            const core::TSQueryNode *root = tsquery.root();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::ROOT_OP:
                {
                    if (!root)
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    std::string op;
                    switch (root->type())
                    {
                        case core::TSQueryNode::Type::LEXEME: op = "LEXEME"; break;
                        case core::TSQueryNode::Type::AND: op = "AND"; break;
                        case core::TSQueryNode::Type::OR: op = "OR"; break;
                        case core::TSQueryNode::Type::NOT: op = "NOT"; break;
                        case core::TSQueryNode::Type::PHRASE: op = "PHRASE"; break;
                    }
                    *out = core::TypedValue::makeText(op);
                    return true;
                }
                case ExtractField::TERMS:
                case ExtractField::OPERATORS:
                case ExtractField::NODES:
                case ExtractField::PHRASE_DISTANCE:
                {
                    std::vector<std::string> terms;
                    std::vector<std::string> ops;
                    int32_t count = 0;
                    collectTsquery(root, terms, ops, count);
                    if (field == ExtractField::TERMS)
                    {
                        std::vector<core::TypedValue> term_vals;
                        term_vals.reserve(terms.size());
                        for (const auto &term : terms)
                        {
                            term_vals.push_back(core::TypedValue::makeText(term));
                        }
                        *out = core::TypedValue::makeArray(term_vals);
                        return true;
                    }
                    if (field == ExtractField::OPERATORS)
                    {
                        std::vector<core::TypedValue> op_vals;
                        op_vals.reserve(ops.size());
                        for (const auto &op : ops)
                        {
                            op_vals.push_back(core::TypedValue::makeText(op));
                        }
                        *out = core::TypedValue::makeArray(op_vals);
                        return true;
                    }
                    if (field == ExtractField::PHRASE_DISTANCE)
                    {
                        int32_t distance = 0;
                        if (root && root->type() == core::TSQueryNode::Type::PHRASE)
                        {
                            distance = root->distance();
                        }
                        *out = core::TypedValue::makeInt32(distance);
                        return true;
                    }
                    *out = core::TypedValue::makeInt32(count);
                    return true;
                }
                default:
                    break;
            }
        }
        if (source_type == core::DataType::INT4RANGE || source_type == core::DataType::INT8RANGE ||
            source_type == core::DataType::NUMRANGE || source_type == core::DataType::TSRANGE ||
            source_type == core::DataType::TSTZRANGE || source_type == core::DataType::DATERANGE)
        {
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::LOWER:
                case ExtractField::LOWER_VALUE:
                case ExtractField::UPPER:
                case ExtractField::UPPER_VALUE:
                case ExtractField::LOWER_INC:
                case ExtractField::UPPER_INC:
                case ExtractField::ISEMPTY:
                case ExtractField::LOWER_INF:
                case ExtractField::UPPER_INF:
                {
                    if (source_type == core::DataType::NUMRANGE)
                    {
                        const auto &range = source.getNumRange();
                        if (field == ExtractField::ISEMPTY)
                        {
                            *out = core::TypedValue::makeBool(range.isEmpty());
                            return true;
                        }
                        if (field == ExtractField::LOWER_INC)
                        {
                            *out = core::TypedValue::makeBool(range.isLowerInclusive());
                            return true;
                        }
                        if (field == ExtractField::UPPER_INC)
                        {
                            *out = core::TypedValue::makeBool(range.isUpperInclusive());
                            return true;
                        }
                        if (field == ExtractField::LOWER_INF)
                        {
                            *out = core::TypedValue::makeBool(!range.isLowerBounded());
                            return true;
                        }
                        if (field == ExtractField::UPPER_INF)
                        {
                            *out = core::TypedValue::makeBool(!range.isUpperBounded());
                            return true;
                        }
                        if (field == ExtractField::LOWER || field == ExtractField::LOWER_VALUE)
                        {
                            if (!range.lower().has_value())
                            {
                                *out = core::TypedValue::makeNull();
                                return true;
                            }
                            *out = core::TypedValue::makeFloat64(*range.lower());
                            return true;
                        }
                        if (field == ExtractField::UPPER || field == ExtractField::UPPER_VALUE)
                        {
                            if (!range.upper().has_value())
                            {
                                *out = core::TypedValue::makeNull();
                                return true;
                            }
                            *out = core::TypedValue::makeFloat64(*range.upper());
                            return true;
                        }
                        break;
                    }
                    if (source_type == core::DataType::INT4RANGE)
                    {
                        const auto &range = source.getInt4Range();
                        if (field == ExtractField::ISEMPTY)
                        {
                            *out = core::TypedValue::makeBool(range.isEmpty());
                            return true;
                        }
                        if (field == ExtractField::LOWER_INC)
                        {
                            *out = core::TypedValue::makeBool(range.isLowerInclusive());
                            return true;
                        }
                        if (field == ExtractField::UPPER_INC)
                        {
                            *out = core::TypedValue::makeBool(range.isUpperInclusive());
                            return true;
                        }
                        if (field == ExtractField::LOWER_INF)
                        {
                            *out = core::TypedValue::makeBool(!range.isLowerBounded());
                            return true;
                        }
                        if (field == ExtractField::UPPER_INF)
                        {
                            *out = core::TypedValue::makeBool(!range.isUpperBounded());
                            return true;
                        }
                        if (field == ExtractField::LOWER || field == ExtractField::LOWER_VALUE)
                        {
                            if (!range.lower().has_value())
                            {
                                *out = core::TypedValue::makeNull();
                                return true;
                            }
                            *out = core::TypedValue::makeInt32(*range.lower());
                            return true;
                        }
                        if (field == ExtractField::UPPER || field == ExtractField::UPPER_VALUE)
                        {
                            if (!range.upper().has_value())
                            {
                                *out = core::TypedValue::makeNull();
                                return true;
                            }
                            *out = core::TypedValue::makeInt32(*range.upper());
                            return true;
                        }
                        break;
                    }
                    const core::Range<int64_t>* range_ptr = nullptr;
                    core::Range<int64_t> range_storage;
                    if (source_type == core::DataType::INT8RANGE)
                    {
                        range_ptr = &source.getInt8Range();
                    }
                    else if (source_type == core::DataType::DATERANGE)
                    {
                        range_ptr = &source.getDateRange<int64_t>();
                    }
                    else if (source_type == core::DataType::TSRANGE)
                    {
                        range_ptr = &source.getTSRange<int64_t>();
                    }
                    else if (source_type == core::DataType::TSTZRANGE)
                    {
                        range_ptr = &source.getTSTZRange<int64_t>();
                    }
                    if (!range_ptr)
                    {
                        break;
                    }
                    const auto &range = *range_ptr;
                    if (field == ExtractField::ISEMPTY)
                    {
                        *out = core::TypedValue::makeBool(range.isEmpty());
                        return true;
                    }
                    if (field == ExtractField::LOWER_INC)
                    {
                        *out = core::TypedValue::makeBool(range.isLowerInclusive());
                        return true;
                    }
                    if (field == ExtractField::UPPER_INC)
                    {
                        *out = core::TypedValue::makeBool(range.isUpperInclusive());
                        return true;
                    }
                    if (field == ExtractField::LOWER_INF)
                    {
                        *out = core::TypedValue::makeBool(!range.isLowerBounded());
                        return true;
                    }
                    if (field == ExtractField::UPPER_INF)
                    {
                        *out = core::TypedValue::makeBool(!range.isUpperBounded());
                        return true;
                    }
                    if (field == ExtractField::LOWER || field == ExtractField::LOWER_VALUE)
                    {
                        if (!range.lower().has_value())
                        {
                            *out = core::TypedValue::makeNull();
                            return true;
                        }
                        int64_t val = *range.lower();
                        if (source_type == core::DataType::INT8RANGE)
                        {
                            *out = core::TypedValue::makeInt64(val);
                        }
                        else if (source_type == core::DataType::DATERANGE)
                        {
                            *out = core::TypedValue::makeDate(val);
                        }
                        else
                        {
                            *out = core::TypedValue::makeTimestamp(val);
                        }
                        return true;
                    }
                    if (field == ExtractField::UPPER || field == ExtractField::UPPER_VALUE)
                    {
                        if (!range.upper().has_value())
                        {
                            *out = core::TypedValue::makeNull();
                            return true;
                        }
                        int64_t val = *range.upper();
                        if (source_type == core::DataType::INT8RANGE)
                        {
                            *out = core::TypedValue::makeInt64(val);
                        }
                        else if (source_type == core::DataType::DATERANGE)
                        {
                            *out = core::TypedValue::makeDate(val);
                        }
                        else
                        {
                            *out = core::TypedValue::makeTimestamp(val);
                        }
                        return true;
                    }
                    break;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::INET || source_type == core::DataType::CIDR)
        {
            const core::InetAddr &inet = (source_type == core::DataType::INET)
                                              ? source.getInet()
                                              : source.getCidr().toInet();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::FAMILY:
                    *out = core::TypedValue::makeInt32(inet.isIPv4() ? 4 : 6);
                    return true;
                case ExtractField::NETMASK:
                    *out = core::TypedValue::makeInt32(inet.netmask());
                    return true;
                case ExtractField::ADDRESS:
                    *out = core::TypedValue::makeText(inet.toStringWithoutNetmask());
                    return true;
                case ExtractField::NETWORK:
                {
                    if (source_type == core::DataType::INET)
                    {
                        *out = core::TypedValue::makeInet(inet.network());
                    }
                    else
                    {
                        *out = core::TypedValue::makeCidr(core::Cidr(inet.network()));
                    }
                    return true;
                }
                case ExtractField::BROADCAST:
                {
                    if (!inet.isIPv4())
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    if (source_type == core::DataType::INET)
                    {
                        *out = core::TypedValue::makeInet(inet.broadcast());
                    }
                    else
                    {
                        *out = core::TypedValue::makeCidr(core::Cidr(inet.broadcast()));
                    }
                    return true;
                }
                case ExtractField::NETMASK_ADDR:
                {
                    if (source_type == core::DataType::INET)
                    {
                        *out = core::TypedValue::makeInet(inet.netmaskAddr());
                    }
                    else
                    {
                        *out = core::TypedValue::makeCidr(core::Cidr(inet.netmaskAddr()));
                    }
                    return true;
                }
                case ExtractField::HOSTMASK:
                {
                    if (source_type == core::DataType::INET)
                    {
                        *out = core::TypedValue::makeInet(inet.hostmask());
                    }
                    else
                    {
                        *out = core::TypedValue::makeCidr(core::Cidr(inet.hostmask()));
                    }
                    return true;
                }
                case ExtractField::IS_IPV4:
                    *out = core::TypedValue::makeBool(inet.isIPv4());
                    return true;
                case ExtractField::IS_IPV6:
                    *out = core::TypedValue::makeBool(inet.isIPv6());
                    return true;
                default:
                    break;
            }
        }

        if (source_type == core::DataType::MACADDR || source_type == core::DataType::MACADDR8)
        {
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::BYTES:
                {
                    std::vector<uint8_t> bytes;
                    if (source_type == core::DataType::MACADDR)
                    {
                        auto mac = source.getMacAddr().bytes();
                        bytes.assign(mac.begin(), mac.end());
                    }
                    else
                    {
                        auto mac = source.getMacAddr8().bytes();
                        bytes.assign(mac.begin(), mac.end());
                    }
                    *out = core::TypedValue::makeBinary(bytes);
                    return true;
                }
                case ExtractField::OUI:
                case ExtractField::TRUNC:
                {
                    std::vector<uint8_t> bytes;
                    if (source_type == core::DataType::MACADDR)
                    {
                        auto mac = source.getMacAddr().bytes();
                        bytes.assign(mac.begin(), mac.begin() + 3);
                    }
                    else
                    {
                        auto mac = source.getMacAddr8().bytes();
                        bytes.assign(mac.begin(), mac.begin() + 3);
                    }
                    *out = core::TypedValue::makeBinary(bytes);
                    return true;
                }
                case ExtractField::NIC:
                {
                    std::vector<uint8_t> bytes;
                    if (source_type == core::DataType::MACADDR)
                    {
                        auto mac = source.getMacAddr().bytes();
                        bytes.assign(mac.begin() + 3, mac.end());
                    }
                    else
                    {
                        auto mac = source.getMacAddr8().bytes();
                        bytes.assign(mac.begin() + 3, mac.end());
                    }
                    *out = core::TypedValue::makeBinary(bytes);
                    return true;
                }
                case ExtractField::IS_MULTICAST:
                {
                    uint8_t first = source_type == core::DataType::MACADDR
                                        ? source.getMacAddr()[0]
                                        : source.getMacAddr8()[0];
                    *out = core::TypedValue::makeBool((first & 0x01) != 0);
                    return true;
                }
                case ExtractField::IS_LOCAL:
                {
                    uint8_t first = source_type == core::DataType::MACADDR
                                        ? source.getMacAddr()[0]
                                        : source.getMacAddr8()[0];
                    *out = core::TypedValue::makeBool((first & 0x02) != 0);
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::POINT)
        {
            core::Point point = source.getPoint();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::X:
                    *out = core::TypedValue::makeFloat64(point.x);
                    return true;
                case ExtractField::Y:
                    *out = core::TypedValue::makeFloat64(point.y);
                    return true;
                case ExtractField::SRID:
                    *out = core::TypedValue::makeInt32(point.srid);
                    return true;
                case ExtractField::BBOX:
                {
                    core::Polygon bbox({{
                        core::Point(point.x, point.y, point.srid),
                        core::Point(point.x, point.y, point.srid),
                        core::Point(point.x, point.y, point.srid),
                        core::Point(point.x, point.y, point.srid),
                        core::Point(point.x, point.y, point.srid)
                    }}, point.srid);
                    *out = core::TypedValue::makePolygon(bbox);
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::LINESTRING)
        {
            core::LineString line = source.getLineString();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::SRID:
                    *out = core::TypedValue::makeInt32(line.srid);
                    return true;
                case ExtractField::POINTS:
                {
                    std::vector<core::TypedValue> points;
                    points.reserve(line.points.size());
                    for (const auto &pt : line.points)
                    {
                        points.push_back(core::TypedValue::makePoint(pt));
                    }
                    *out = core::TypedValue::makeArray(points);
                    return true;
                }
                case ExtractField::NUM_POINTS:
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(line.points.size()));
                    return true;
                case ExtractField::LENGTH:
                {
                    double length = 0.0;
                    for (size_t i = 1; i < line.points.size(); ++i)
                    {
                        double dx = line.points[i].x - line.points[i - 1].x;
                        double dy = line.points[i].y - line.points[i - 1].y;
                        length += std::sqrt(dx * dx + dy * dy);
                    }
                    *out = core::TypedValue::makeFloat64(length);
                    return true;
                }
                case ExtractField::BBOX:
                {
                    if (line.points.empty())
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    double min_x = line.points[0].x;
                    double max_x = line.points[0].x;
                    double min_y = line.points[0].y;
                    double max_y = line.points[0].y;
                    for (const auto &pt : line.points)
                    {
                        min_x = std::min(min_x, pt.x);
                        max_x = std::max(max_x, pt.x);
                        min_y = std::min(min_y, pt.y);
                        max_y = std::max(max_y, pt.y);
                    }
                    std::vector<core::Point> ring = {
                        core::Point(min_x, min_y, line.srid),
                        core::Point(max_x, min_y, line.srid),
                        core::Point(max_x, max_y, line.srid),
                        core::Point(min_x, max_y, line.srid),
                        core::Point(min_x, min_y, line.srid)
                    };
                    core::Polygon bbox({ring}, line.srid);
                    *out = core::TypedValue::makePolygon(bbox);
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::POLYGON)
        {
            core::Polygon poly = source.getPolygon();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::SRID:
                    *out = core::TypedValue::makeInt32(poly.srid);
                    return true;
                case ExtractField::RINGS:
                {
                    std::vector<core::TypedValue> rings_out;
                    rings_out.reserve(poly.rings.size());
                    for (const auto &ring : poly.rings)
                    {
                        std::vector<core::TypedValue> points;
                        points.reserve(ring.size());
                        for (const auto &pt : ring)
                        {
                            points.push_back(core::TypedValue::makePoint(pt));
                        }
                        rings_out.push_back(core::TypedValue::makeArray(points));
                    }
                    *out = core::TypedValue::makeArray(rings_out);
                    return true;
                }
                case ExtractField::NUM_RINGS:
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(poly.rings.size()));
                    return true;
                case ExtractField::AREA:
                {
                    if (poly.rings.empty())
                    {
                        *out = core::TypedValue::makeFloat64(0.0);
                        return true;
                    }
                    auto ringArea = [](const std::vector<core::Point> &ring) -> double {
                        if (ring.size() < 3) return 0.0;
                        double area = 0.0;
                        for (size_t i = 0; i + 1 < ring.size(); ++i)
                        {
                            area += ring[i].x * ring[i + 1].y - ring[i + 1].x * ring[i].y;
                        }
                        return 0.5 * area;
                    };
                    double area = ringArea(poly.rings[0]);
                    for (size_t i = 1; i < poly.rings.size(); ++i)
                    {
                        area -= ringArea(poly.rings[i]);
                    }
                    *out = core::TypedValue::makeFloat64(std::abs(area));
                    return true;
                }
                case ExtractField::BBOX:
                {
                    if (poly.rings.empty() || poly.rings[0].empty())
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    double min_x = poly.rings[0][0].x;
                    double max_x = poly.rings[0][0].x;
                    double min_y = poly.rings[0][0].y;
                    double max_y = poly.rings[0][0].y;
                    for (const auto &ring : poly.rings)
                    {
                        for (const auto &pt : ring)
                        {
                            min_x = std::min(min_x, pt.x);
                            max_x = std::max(max_x, pt.x);
                            min_y = std::min(min_y, pt.y);
                            max_y = std::max(max_y, pt.y);
                        }
                    }
                    std::vector<core::Point> ring = {
                        core::Point(min_x, min_y, poly.srid),
                        core::Point(max_x, min_y, poly.srid),
                        core::Point(max_x, max_y, poly.srid),
                        core::Point(min_x, max_y, poly.srid),
                        core::Point(min_x, min_y, poly.srid)
                    };
                    core::Polygon bbox({ring}, poly.srid);
                    *out = core::TypedValue::makePolygon(bbox);
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::MULTIPOINT)
        {
            core::MultiPoint mp = source.getMultiPoint();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::SRID:
                    *out = core::TypedValue::makeInt32(mp.srid);
                    return true;
                case ExtractField::GEOMETRIES:
                {
                    std::vector<core::TypedValue> geoms;
                    geoms.reserve(mp.points.size());
                    for (const auto &pt : mp.points)
                    {
                        geoms.push_back(core::TypedValue::makePoint(pt));
                    }
                    *out = core::TypedValue::makeArray(geoms);
                    return true;
                }
                case ExtractField::NUM_GEOMETRIES:
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(mp.points.size()));
                    return true;
                case ExtractField::BBOX:
                {
                    if (mp.points.empty())
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    double min_x = mp.points[0].x;
                    double max_x = mp.points[0].x;
                    double min_y = mp.points[0].y;
                    double max_y = mp.points[0].y;
                    for (const auto &pt : mp.points)
                    {
                        min_x = std::min(min_x, pt.x);
                        max_x = std::max(max_x, pt.x);
                        min_y = std::min(min_y, pt.y);
                        max_y = std::max(max_y, pt.y);
                    }
                    std::vector<core::Point> ring = {
                        core::Point(min_x, min_y, mp.srid),
                        core::Point(max_x, min_y, mp.srid),
                        core::Point(max_x, max_y, mp.srid),
                        core::Point(min_x, max_y, mp.srid),
                        core::Point(min_x, min_y, mp.srid)
                    };
                    core::Polygon bbox({ring}, mp.srid);
                    *out = core::TypedValue::makePolygon(bbox);
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::MULTILINESTRING)
        {
            core::MultiLineString mls = source.getMultiLineString();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::SRID:
                    *out = core::TypedValue::makeInt32(mls.srid);
                    return true;
                case ExtractField::GEOMETRIES:
                {
                    std::vector<core::TypedValue> geoms;
                    geoms.reserve(mls.linestrings.size());
                    for (const auto &line : mls.linestrings)
                    {
                        geoms.push_back(core::TypedValue::makeLineString(line));
                    }
                    *out = core::TypedValue::makeArray(geoms);
                    return true;
                }
                case ExtractField::NUM_GEOMETRIES:
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(mls.linestrings.size()));
                    return true;
                case ExtractField::BBOX:
                {
                    if (mls.linestrings.empty() || mls.linestrings[0].points.empty())
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    bool first = true;
                    double min_x = 0.0;
                    double max_x = 0.0;
                    double min_y = 0.0;
                    double max_y = 0.0;
                    for (const auto &line : mls.linestrings)
                    {
                        for (const auto &pt : line.points)
                        {
                            if (first)
                            {
                                min_x = max_x = pt.x;
                                min_y = max_y = pt.y;
                                first = false;
                            }
                            else
                            {
                                min_x = std::min(min_x, pt.x);
                                max_x = std::max(max_x, pt.x);
                                min_y = std::min(min_y, pt.y);
                                max_y = std::max(max_y, pt.y);
                            }
                        }
                    }
                    std::vector<core::Point> ring = {
                        core::Point(min_x, min_y, mls.srid),
                        core::Point(max_x, min_y, mls.srid),
                        core::Point(max_x, max_y, mls.srid),
                        core::Point(min_x, max_y, mls.srid),
                        core::Point(min_x, min_y, mls.srid)
                    };
                    core::Polygon bbox({ring}, mls.srid);
                    *out = core::TypedValue::makePolygon(bbox);
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::MULTIPOLYGON)
        {
            core::MultiPolygon mp = source.getMultiPolygon();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::SRID:
                    *out = core::TypedValue::makeInt32(mp.srid);
                    return true;
                case ExtractField::GEOMETRIES:
                {
                    std::vector<core::TypedValue> geoms;
                    geoms.reserve(mp.polygons.size());
                    for (const auto &poly : mp.polygons)
                    {
                        geoms.push_back(core::TypedValue::makePolygon(poly));
                    }
                    *out = core::TypedValue::makeArray(geoms);
                    return true;
                }
                case ExtractField::NUM_GEOMETRIES:
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(mp.polygons.size()));
                    return true;
                case ExtractField::BBOX:
                {
                    bool first = true;
                    double min_x = 0.0;
                    double max_x = 0.0;
                    double min_y = 0.0;
                    double max_y = 0.0;
                    for (const auto &poly : mp.polygons)
                    {
                        for (const auto &ring : poly.rings)
                        {
                            for (const auto &pt : ring)
                            {
                                if (first)
                                {
                                    min_x = max_x = pt.x;
                                    min_y = max_y = pt.y;
                                    first = false;
                                }
                                else
                                {
                                    min_x = std::min(min_x, pt.x);
                                    max_x = std::max(max_x, pt.x);
                                    min_y = std::min(min_y, pt.y);
                                    max_y = std::max(max_y, pt.y);
                                }
                            }
                        }
                    }
                    if (first)
                    {
                        *out = core::TypedValue::makeNull();
                        return true;
                    }
                    std::vector<core::Point> ring = {
                        core::Point(min_x, min_y, mp.srid),
                        core::Point(max_x, min_y, mp.srid),
                        core::Point(max_x, max_y, mp.srid),
                        core::Point(min_x, max_y, mp.srid),
                        core::Point(min_x, min_y, mp.srid)
                    };
                    core::Polygon bbox({ring}, mp.srid);
                    *out = core::TypedValue::makePolygon(bbox);
                    return true;
                }
                default:
                    break;
            }
        }

        if (source_type == core::DataType::GEOMETRYCOLLECTION)
        {
            const auto &collection = source.getGeometryCollection();
            switch (field)
            {
                case ExtractField::VALUE:
                    *out = source;
                    return true;
                case ExtractField::SRID:
                    *out = core::TypedValue::makeInt32(collection.srid);
                    return true;
                case ExtractField::GEOMETRIES:
                {
                    std::vector<core::TypedValue> geoms;
                    geoms.reserve(collection.geometries.size());
                    for (const auto &geom : collection.geometries)
                    {
                        if (geom)
                        {
                            geoms.push_back(*geom);
                        }
                    }
                    *out = core::TypedValue::makeArray(geoms);
                    return true;
                }
                case ExtractField::NUM_GEOMETRIES:
                    *out = core::TypedValue::makeInt32(static_cast<int32_t>(collection.geometries.size()));
                    return true;
                default:
                    break;
            }
        }

        if (error)
        {
            std::ostringstream oss;
            oss << "EXTRACT not supported for " << static_cast<int>(source_type)
                << " field " << extractFieldToString(field);
            *error = oss.str();
        }
        return false;
    }
    bool alterElement(const core::TypedValue& source,
                      ExtractField field,
                      const std::vector<core::TypedValue>& args,
                      const core::TypedValue& new_value,
                      core::TypedValue* out,
                      std::string* error)
    {
        if (!out)
        {
            if (error) *error = "Missing output";
            return false;
        }
        if (source.isNull())
        {
            *out = core::TypedValue::makeNull();
            return true;
        }

        ElementArgSpec arg_spec = extractFieldArgSpec(field);
        if (args.size() < arg_spec.min_args || args.size() > arg_spec.max_args)
        {
            if (error)
            {
                std::ostringstream oss;
                oss << "Invalid argument count for ALTER_ELEMENT(" << extractFieldToString(field)
                    << ")";
                *error = oss.str();
            }
            return false;
        }

        if (isReadOnlyField(field))
        {
            if (error) *error = "ALTER_ELEMENT target is read-only";
            return false;
        }

        core::DataType source_type = source.type();

        if (source_type == core::DataType::UNKNOWN || source_type == core::DataType::NULL_TYPE)
        {
            if (error) *error = "ALTER_ELEMENT not supported for UNKNOWN type";
            return false;
        }

        if (field == ExtractField::VALUE)
        {
            core::TypeInfo info(source_type);
            core::TypedValue casted;
            if (!castValue(new_value, info, casted, error))
            {
                return false;
            }
            *out = casted;
            return true;
        }

        if (source_type == core::DataType::DATE || source_type == core::DataType::TIME ||
            source_type == core::DataType::TIMESTAMP)
        {
            int32_t offset_seconds = source.getTimezoneOffsetSeconds();
            if (field == ExtractField::TIMEZONE || field == ExtractField::TZ_OFFSET ||
                field == ExtractField::TIMEZONE_HOUR || field == ExtractField::TIMEZONE_MINUTE)
            {
                if (new_value.isNull())
                {
                    if (error) *error = "Timezone component cannot be NULL";
                    return false;
                }
                int32_t total_minutes = offset_seconds / 60;
                int32_t hour = total_minutes / 60;
                int32_t minute = std::abs(total_minutes % 60);
                int32_t sign = offset_seconds < 0 ? -1 : 1;
                if (field == ExtractField::TIMEZONE_HOUR)
                {
                    int32_t new_hour = new_value.toInt32();
                    sign = new_hour == 0 ? sign : (new_hour < 0 ? -1 : 1);
                    hour = new_hour;
                }
                else if (field == ExtractField::TIMEZONE_MINUTE)
                {
                    int32_t new_min = new_value.toInt32();
                    if (new_min < 0)
                    {
                        sign = -1;
                        new_min = std::abs(new_min);
                    }
                    minute = new_min;
                }
                else
                {
                    offset_seconds = new_value.toInt32();
                    hour = offset_seconds / 3600;
                    minute = std::abs((offset_seconds / 60) % 60);
                    sign = offset_seconds < 0 ? -1 : 1;
                }
                if (field != ExtractField::TZ_OFFSET && field != ExtractField::TIMEZONE)
                {
                    if (minute < 0 || minute > 59)
                    {
                        if (error) *error = "Invalid timezone minute";
                        return false;
                    }
                    offset_seconds = (hour * 3600) + sign * minute * 60;
                }

                if (source_type == core::DataType::DATE)
                {
                    int64_t local_days = localDaysFromDate(source.getDate(), source.getTimezoneOffsetSeconds());
                    int32_t year = core::TypeExtractor::extractYear(local_days);
                    int32_t month = core::TypeExtractor::extractMonth(local_days);
                    int32_t day = core::TypeExtractor::extractDay(local_days);
                    if (!buildTemporalFromParts(source_type, year, month, day, 0, 0, 0, 0,
                                                offset_seconds, *out, error))
                    {
                        return false;
                    }
                    return true;
                }

                int64_t utc_micros = source_type == core::DataType::TIME ? source.getTime()
                                                                         : source.getTimestamp();
                int64_t local_micros = utc_micros +
                                       static_cast<int64_t>(source.getTimezoneOffsetSeconds()) *
                                           kMicrosPerSecond;
                if (source_type == core::DataType::TIME)
                {
                    local_micros = normalizeTimeOfDay(local_micros);
                }
                int64_t new_utc = local_micros -
                                  static_cast<int64_t>(offset_seconds) * kMicrosPerSecond;
                if (source_type == core::DataType::TIME)
                {
                    new_utc = normalizeTimeOfDay(new_utc);
                    *out = core::TypedValue::makeTime(new_utc, offset_seconds);
                }
                else
                {
                    *out = core::TypedValue::makeTimestamp(new_utc, offset_seconds);
                }
                return true;
            }

            if (new_value.isNull())
            {
                if (error) *error = "Temporal component cannot be NULL";
                return false;
            }

            int32_t year = 0;
            int32_t month = 0;
            int32_t day = 0;
            int32_t hour = 0;
            int32_t minute = 0;
            int32_t second = 0;
            int32_t micro = 0;

            if (source_type == core::DataType::DATE)
            {
                int64_t local_days = localDaysFromDate(source.getDate(), offset_seconds);
                year = core::TypeExtractor::extractYear(local_days);
                month = core::TypeExtractor::extractMonth(local_days);
                day = core::TypeExtractor::extractDay(local_days);
            }
            else if (source_type == core::DataType::TIME)
            {
                int64_t local_micros = normalizeTimeOfDay(source.getTime() +
                                                          static_cast<int64_t>(offset_seconds) *
                                                              kMicrosPerSecond);
                splitLocalTime(local_micros, hour, minute, second, micro);
            }
            else
            {
                int64_t local_micros = source.getTimestamp() +
                                       static_cast<int64_t>(offset_seconds) * kMicrosPerSecond;
                int64_t local_seconds = floorDiv(local_micros, kMicrosPerSecond);
                int64_t local_days = floorDiv(local_seconds, kSecondsPerDay);
                year = core::TypeExtractor::extractYear(local_days);
                month = core::TypeExtractor::extractMonth(local_days);
                day = core::TypeExtractor::extractDay(local_days);
                splitLocalTime(local_micros, hour, minute, second, micro);
            }

            int32_t new_part = new_value.toInt32();
            switch (field)
            {
                case ExtractField::YEAR: year = new_part; break;
                case ExtractField::MONTH: month = new_part; break;
                case ExtractField::DAY: day = new_part; break;
                case ExtractField::HOUR: hour = new_part; break;
                case ExtractField::MINUTE: minute = new_part; break;
                case ExtractField::SECOND: second = new_part; break;
                case ExtractField::MILLISECOND:
                    micro = new_part * 1000 + (micro % 1000);
                    break;
                case ExtractField::MICROSECOND:
                    micro = new_part;
                    break;
                default:
                    if (error) *error = "Unsupported temporal element";
                    return false;
            }

            if (!buildTemporalFromParts(source_type, year, month, day, hour, minute, second, micro,
                                        offset_seconds, *out, error))
            {
                return false;
            }
            return true;
        }

        if (source_type == core::DataType::INTERVAL)
        {
            core::Interval interval = source.getInterval();
            if (new_value.isNull())
            {
                if (error) *error = "Interval component cannot be NULL";
                return false;
            }
            int64_t val = new_value.toInt64();
            switch (field)
            {
                case ExtractField::YEAR:
                {
                    int32_t remainder = interval.months % 12;
                    interval.months = static_cast<int32_t>(val * 12 + remainder);
                    break;
                }
                case ExtractField::MONTH:
                {
                    int32_t year = static_cast<int32_t>(floorDiv(interval.months, 12));
                    interval.months = year * 12 + static_cast<int32_t>(val);
                    break;
                }
                case ExtractField::DAY:
                    interval.days = static_cast<int32_t>(val);
                    break;
                case ExtractField::HOUR:
                case ExtractField::MINUTE:
                case ExtractField::SECOND:
                case ExtractField::MILLISECOND:
                case ExtractField::MICROSECOND:
                {
                    int64_t micros = interval.microseconds;
                    int64_t sign = micros < 0 ? -1 : 1;
                    int64_t abs_micros = std::llabs(micros);
                    int64_t total_seconds = abs_micros / kMicrosPerSecond;
                    int64_t micro_part = abs_micros % kMicrosPerSecond;
                    int64_t hours = total_seconds / 3600;
                    int64_t minutes = (total_seconds % 3600) / 60;
                    int64_t seconds = total_seconds % 60;
                    if (field == ExtractField::HOUR) hours = val;
                    if (field == ExtractField::MINUTE) minutes = val;
                    if (field == ExtractField::SECOND) seconds = val;
                    if (field == ExtractField::MILLISECOND) micro_part = val * 1000 + (micro_part % 1000);
                    if (field == ExtractField::MICROSECOND) micro_part = val;
                    int64_t new_total_seconds = hours * 3600 + minutes * 60 + seconds;
                    interval.microseconds = sign * (new_total_seconds * kMicrosPerSecond + micro_part);
                    break;
                }
                default:
                    if (error) *error = "Unsupported interval element";
                    return false;
            }
            *out = core::TypedValue::makeInterval(interval);
            return true;
        }
        if (source_type == core::DataType::UUID)
        {
            if (field == ExtractField::BYTES)
            {
                std::vector<uint8_t> bytes;
                if (!decodeBinaryValue(new_value, bytes, error))
                {
                    return false;
                }
                if (bytes.size() != 16)
                {
                    if (error) *error = "UUID bytes must be 16 bytes";
                    return false;
                }
                *out = core::TypedValue::makeUUID(bytes);
                return true;
            }
            if (error) *error = "ALTER_ELEMENT not supported for UUID element";
            return false;
        }

        if (source_type == core::DataType::JSON || source_type == core::DataType::JSONB)
        {
            if (field == ExtractField::PATH)
            {
                std::string path;
                if (!parseStringArg(args, 0, path, error))
                {
                    return false;
                }
                json doc;
                if (!parseJson(source, doc, error))
                {
                    return false;
                }
                json new_json = valueToJson(new_value);
                std::vector<std::string> components = parseJsonPath(path);
                if (!setJsonPath(doc, components, new_json, error))
                {
                    return false;
                }
                *out = jsonToValue(doc, source_type);
                return true;
            }
            if (error) *error = "ALTER_ELEMENT not supported for JSON element";
            return false;
        }

        if (source_type == core::DataType::XML)
        {
            if (field == ExtractField::PATH)
            {
#ifdef HAVE_LIBXML2
                std::string xml_text = source.toString();
                std::string xpath;
                if (!parseStringArg(args, 0, xpath, error))
                {
                    return false;
                }
                xmlDocPtr doc = xmlReadMemory(xml_text.c_str(),
                                              static_cast<int>(xml_text.size()),
                                              nullptr, nullptr,
                                              XML_PARSE_NONET | XML_PARSE_NOENT);
                if (!doc)
                {
                    if (error) *error = "Invalid XML";
                    return false;
                }
                xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
                if (!xpathCtx)
                {
                    xmlFreeDoc(doc);
                    if (error) *error = "XML XPath context error";
                    return false;
                }
                xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(
                    reinterpret_cast<const xmlChar*>(xpath.c_str()), xpathCtx);
                if (!xpathObj || xpathObj->type != XPATH_NODESET ||
                    !xpathObj->nodesetval || xpathObj->nodesetval->nodeNr == 0)
                {
                    xmlXPathFreeObject(xpathObj);
                    xmlXPathFreeContext(xpathCtx);
                    xmlFreeDoc(doc);
                    if (error) *error = "XPath not found";
                    return false;
                }
                std::string new_xml = new_value.toString();
                xmlDocPtr new_doc = xmlReadMemory(new_xml.c_str(),
                                                  static_cast<int>(new_xml.size()),
                                                  nullptr, nullptr,
                                                  XML_PARSE_NONET | XML_PARSE_NOENT | XML_PARSE_NOERROR);
                for (int i = 0; i < xpathObj->nodesetval->nodeNr; ++i)
                {
                    xmlNodePtr node = xpathObj->nodesetval->nodeTab[i];
                    if (new_doc)
                    {
                        xmlNodePtr new_root = xmlDocGetRootElement(new_doc);
                        if (new_root)
                        {
                            xmlNodePtr imported = xmlDocCopyNode(new_root, doc, 1);
                            xmlReplaceNode(node, imported);
                            xmlFreeNode(node);
                            continue;
                        }
                    }
                    xmlNodeSetContent(node, reinterpret_cast<const xmlChar*>(new_xml.c_str()));
                }
                if (new_doc)
                {
                    xmlFreeDoc(new_doc);
                }
                xmlXPathFreeObject(xpathObj);
                xmlXPathFreeContext(xpathCtx);
                xmlChar *buf = nullptr;
                int buf_size = 0;
                xmlDocDumpMemory(doc, &buf, &buf_size);
                std::string result;
                if (buf && buf_size > 0)
                {
                    result.assign(reinterpret_cast<const char*>(buf), buf_size);
                }
                if (buf)
                {
                    xmlFree(buf);
                }
                xmlFreeDoc(doc);
                *out = core::TypedValue::makeXML(result);
                return true;
#else
                if (error) *error = "XML support not available";
                return false;
#endif
            }
            if (error) *error = "ALTER_ELEMENT not supported for XML element";
            return false;
        }

        if (source_type == core::DataType::BINARY || source_type == core::DataType::VARBINARY ||
            source_type == core::DataType::BLOB || source_type == core::DataType::BYTEA)
        {
            std::vector<uint8_t> data = source.getBinary();
            if (field == ExtractField::BYTE)
            {
                int64_t index = 0;
                if (!parseIntegerArg(args, 0, index, error))
                {
                    return false;
                }
                if (new_value.isNull())
                {
                    if (error) *error = "BYTE cannot be NULL";
                    return false;
                }
                if (index < 0 || static_cast<size_t>(index) >= data.size())
                {
                    if (error) *error = "BYTE index out of range";
                    return false;
                }
                data[static_cast<size_t>(index)] = static_cast<uint8_t>(new_value.toInt64());
            }
            else if (field == ExtractField::BIT)
            {
                int64_t index = 0;
                if (!parseIntegerArg(args, 0, index, error))
                {
                    return false;
                }
                if (new_value.isNull())
                {
                    if (error) *error = "BIT cannot be NULL";
                    return false;
                }
                if (index < 0 || static_cast<size_t>(index / 8) >= data.size())
                {
                    if (error) *error = "BIT index out of range";
                    return false;
                }
                size_t byte_index = static_cast<size_t>(index / 8);
                int bit_index = static_cast<int>(index % 8);
                uint8_t mask = static_cast<uint8_t>(1u << (7 - bit_index));
                if (new_value.toInt64() != 0)
                {
                    data[byte_index] |= mask;
                }
                else
                {
                    data[byte_index] &= static_cast<uint8_t>(~mask);
                }
            }
            else if (field == ExtractField::SLICE)
            {
                int64_t start = 0;
                int64_t length = 0;
                if (!parseIntegerArg(args, 0, start, error) ||
                    !parseIntegerArg(args, 1, length, error))
                {
                    return false;
                }
                if (start < 0 || length < 0)
                {
                    if (error) *error = "Invalid slice bounds";
                    return false;
                }
                std::vector<uint8_t> replacement;
                if (!decodeBinaryValue(new_value, replacement, error))
                {
                    return false;
                }
                size_t start_idx = static_cast<size_t>(start);
                size_t end_idx = std::min(data.size(), start_idx + static_cast<size_t>(length));
                if (start_idx > data.size())
                {
                    if (error) *error = "Slice start out of range";
                    return false;
                }
                if (source_type == core::DataType::BINARY && replacement.size() != (end_idx - start_idx))
                {
                    if (error) *error = "Slice length mismatch for BINARY";
                    return false;
                }
                data.erase(data.begin() + start_idx, data.begin() + end_idx);
                data.insert(data.begin() + start_idx, replacement.begin(), replacement.end());
            }
            else
            {
                if (error) *error = "ALTER_ELEMENT not supported for binary element";
                return false;
            }

            if (source_type == core::DataType::BLOB)
            {
                *out = core::TypedValue::makeBlob(data);
            }
            else if (source_type == core::DataType::BYTEA)
            {
                *out = core::TypedValue::makeBytea(data);
            }
            else if (source_type == core::DataType::VARBINARY)
            {
                *out = core::TypedValue::makeVarbinary(data);
            }
            else
            {
                *out = core::TypedValue::makeBinary(data);
            }
            return true;
        }

        if (source_type == core::DataType::VECTOR)
        {
            std::vector<float> vec;
            if (!decodeVectorValue(source, vec, error))
            {
                return false;
            }
            if (field == ExtractField::ELEMENT)
            {
                int64_t index = 0;
                if (!parseIntegerArg(args, 0, index, error))
                {
                    return false;
                }
                if (index <= 0 || static_cast<size_t>(index) > vec.size())
                {
                    if (error) *error = "Vector index out of range";
                    return false;
                }
                if (new_value.isNull())
                {
                    if (error) *error = "Vector element cannot be NULL";
                    return false;
                }
                vec[static_cast<size_t>(index - 1)] = new_value.toFloat();
                *out = core::TypedValue::makeVector(vec);
                return true;
            }
            if (error) *error = "ALTER_ELEMENT not supported for vector element";
            return false;
        }

        if (source_type == core::DataType::ARRAY)
        {
            std::vector<core::TypedValue> elements = source.getArray();
            if (field == ExtractField::ELEMENT)
            {
                int64_t index = 0;
                if (!parseIntegerArg(args, 0, index, error))
                {
                    return false;
                }
                if (index <= 0 || static_cast<size_t>(index) > elements.size())
                {
                    if (error) *error = "Array index out of range";
                    return false;
                }
                elements[static_cast<size_t>(index - 1)] = new_value;
                *out = core::TypedValue::makeArray(elements);
                return true;
            }
            if (error) *error = "ALTER_ELEMENT not supported for array element";
            return false;
        }

        if (source_type == core::DataType::COMPOSITE)
        {
            std::vector<core::TypedValue> values = source.getCompositeValues();
            if (field == ExtractField::FIELD)
            {
                if (args.empty())
                {
                    if (error) *error = "FIELD requires a selector";
                    return false;
                }
                size_t index = std::numeric_limits<size_t>::max();
                if (!args[0].isNull() && (args[0].type() == core::DataType::INT32 ||
                                          args[0].type() == core::DataType::INT64 ||
                                          args[0].type() == core::DataType::INT16 ||
                                          args[0].type() == core::DataType::INT8))
                {
                    int64_t idx = args[0].toInt64();
                    if (idx > 0)
                    {
                        index = static_cast<size_t>(idx - 1);
                    }
                }
                else
                {
                    std::string name = args[0].toString();
                    std::vector<std::string> names = source.getCompositeFieldNames();
                    std::string normalized = normalizeIdentifier(name);
                    for (size_t i = 0; i < names.size(); ++i)
                    {
                        if (normalizeIdentifier(names[i]) == normalized)
                        {
                            index = i;
                            break;
                        }
                    }
                }
                if (index >= values.size())
                {
                    if (error) *error = "Composite field not found";
                    return false;
                }
                values[index] = new_value;
                *out = core::TypedValue::makeComposite(source.getCompositeFieldNames(), values);
                return true;
            }
            if (error) *error = "ALTER_ELEMENT not supported for composite element";
            return false;
        }

        if (source_type == core::DataType::VARIANT)
        {
            if (field == ExtractField::VALUE)
            {
                auto tag = source.getVariantTag();
                if (tag.has_value())
                {
                    core::TypeInfo info(tag.value());
                    core::TypedValue casted;
                    if (!castValue(new_value, info, casted, error))
                    {
                        return false;
                    }
                    *out = core::TypedValue::makeVariant(tag.value(), casted);
                }
                else
                {
                    *out = core::TypedValue::makeVariant(new_value);
                }
                return true;
            }
            if (field == ExtractField::DATATYPE)
            {
                if (new_value.isNull())
                {
                    if (error) *error = "DATATYPE cannot be NULL";
                    return false;
                }
                int32_t type_code = new_value.toInt32();
                core::DataType new_type = static_cast<core::DataType>(type_code);
                core::TypeInfo info(new_type);
                core::TypedValue casted;
                if (!castValue(source.getVariantValue(), info, casted, error))
                {
                    return false;
                }
                *out = core::TypedValue::makeVariant(new_type, casted);
                return true;
            }
            if (error) *error = "ALTER_ELEMENT not supported for variant element";
            return false;
        }

        if (source_type == core::DataType::INT4RANGE || source_type == core::DataType::INT8RANGE ||
            source_type == core::DataType::NUMRANGE || source_type == core::DataType::TSRANGE ||
            source_type == core::DataType::TSTZRANGE || source_type == core::DataType::DATERANGE)
        {
            if (field == ExtractField::LOWER || field == ExtractField::LOWER_VALUE ||
                field == ExtractField::UPPER || field == ExtractField::UPPER_VALUE ||
                field == ExtractField::LOWER_INC || field == ExtractField::UPPER_INC ||
                field == ExtractField::ISEMPTY || field == ExtractField::LOWER_INF ||
                field == ExtractField::UPPER_INF)
            {
                if (source_type == core::DataType::NUMRANGE)
                {
                    auto range = source.getNumRange();
                    if (field == ExtractField::ISEMPTY)
                    {
                        bool empty = false;
                        if (!toBool(new_value, empty, error))
                        {
                            return false;
                        }
                        range = empty ? core::Range<double>() : range;
                        *out = core::TypedValue::makeNumRange(range);
                        return true;
                    }
                    if (field == ExtractField::LOWER_INC || field == ExtractField::UPPER_INC)
                    {
                        bool flag = false;
                        if (!toBool(new_value, flag, error))
                        {
                            return false;
                        }
                        range = core::Range<double>(range.lower(), range.upper(),
                                                    field == ExtractField::LOWER_INC ? flag : range.isLowerInclusive(),
                                                    field == ExtractField::UPPER_INC ? flag : range.isUpperInclusive());
                        *out = core::TypedValue::makeNumRange(range);
                        return true;
                    }
                    if (field == ExtractField::LOWER_INF || field == ExtractField::UPPER_INF)
                    {
                        bool flag = false;
                        if (!toBool(new_value, flag, error))
                        {
                            return false;
                        }
                        std::optional<double> lower = range.lower();
                        std::optional<double> upper = range.upper();
                        if (field == ExtractField::LOWER_INF && flag)
                        {
                            lower.reset();
                        }
                        if (field == ExtractField::UPPER_INF && flag)
                        {
                            upper.reset();
                        }
                        range = core::Range<double>(lower, upper,
                                                    range.isLowerInclusive(), range.isUpperInclusive());
                        *out = core::TypedValue::makeNumRange(range);
                        return true;
                    }
                    if (field == ExtractField::LOWER || field == ExtractField::LOWER_VALUE)
                    {
                        std::optional<double> lower;
                        if (!new_value.isNull())
                        {
                            lower = new_value.toDouble();
                        }
                        range = core::Range<double>(lower, range.upper(),
                                                    range.isLowerInclusive(), range.isUpperInclusive());
                        *out = core::TypedValue::makeNumRange(range);
                        return true;
                    }
                    if (field == ExtractField::UPPER || field == ExtractField::UPPER_VALUE)
                    {
                        std::optional<double> upper;
                        if (!new_value.isNull())
                        {
                            upper = new_value.toDouble();
                        }
                        range = core::Range<double>(range.lower(), upper,
                                                    range.isLowerInclusive(), range.isUpperInclusive());
                        *out = core::TypedValue::makeNumRange(range);
                        return true;
                    }
                }
                else if (source_type == core::DataType::INT4RANGE)
                {
                    auto range = source.getInt4Range();
                    if (field == ExtractField::ISEMPTY)
                    {
                        bool empty = false;
                        if (!toBool(new_value, empty, error))
                        {
                            return false;
                        }
                        if (empty)
                        {
                            range = core::Range<int32_t>();
                        }
                        *out = core::TypedValue::makeInt4Range(range);
                        return true;
                    }
                    if (field == ExtractField::LOWER_INC || field == ExtractField::UPPER_INC)
                    {
                        bool flag = false;
                        if (!toBool(new_value, flag, error))
                        {
                            return false;
                        }
                        range = core::Range<int32_t>(range.lower(), range.upper(),
                                                     field == ExtractField::LOWER_INC ? flag : range.isLowerInclusive(),
                                                     field == ExtractField::UPPER_INC ? flag : range.isUpperInclusive());
                    }
                    else if (field == ExtractField::LOWER_INF || field == ExtractField::UPPER_INF)
                    {
                        bool flag = false;
                        if (!toBool(new_value, flag, error))
                        {
                            return false;
                        }
                        std::optional<int32_t> lower = range.lower();
                        std::optional<int32_t> upper = range.upper();
                        if (field == ExtractField::LOWER_INF && flag)
                        {
                            lower.reset();
                        }
                        if (field == ExtractField::UPPER_INF && flag)
                        {
                            upper.reset();
                        }
                        range = core::Range<int32_t>(lower, upper,
                                                     range.isLowerInclusive(), range.isUpperInclusive());
                    }
                    else if (field == ExtractField::LOWER || field == ExtractField::LOWER_VALUE)
                    {
                        std::optional<int32_t> lower;
                        if (!new_value.isNull())
                        {
                            lower = static_cast<int32_t>(new_value.toInt32());
                        }
                        range = core::Range<int32_t>(lower, range.upper(),
                                                     range.isLowerInclusive(), range.isUpperInclusive());
                    }
                    else if (field == ExtractField::UPPER || field == ExtractField::UPPER_VALUE)
                    {
                        std::optional<int32_t> upper;
                        if (!new_value.isNull())
                        {
                            upper = static_cast<int32_t>(new_value.toInt32());
                        }
                        range = core::Range<int32_t>(range.lower(), upper,
                                                     range.isLowerInclusive(), range.isUpperInclusive());
                    }
                    *out = core::TypedValue::makeInt4Range(range);
                    return true;
                }
                else
                {
                    core::Range<int64_t> range = (source_type == core::DataType::INT8RANGE)
                                                     ? source.getInt8Range()
                                                     : (source_type == core::DataType::DATERANGE)
                                                           ? source.getDateRange<int64_t>()
                                                           : (source_type == core::DataType::TSRANGE)
                                                                 ? source.getTSRange<int64_t>()
                                                                 : source.getTSTZRange<int64_t>();
                    if (field == ExtractField::ISEMPTY)
                    {
                        bool empty = false;
                        if (!toBool(new_value, empty, error))
                        {
                            return false;
                        }
                        if (empty)
                        {
                            range = core::Range<int64_t>();
                        }
                        if (source_type == core::DataType::INT8RANGE)
                        {
                            *out = core::TypedValue::makeInt8Range(range);
                        }
                        else if (source_type == core::DataType::DATERANGE)
                        {
                            *out = core::TypedValue::makeDateRange<int64_t>(range);
                        }
                        else if (source_type == core::DataType::TSRANGE)
                        {
                            *out = core::TypedValue::makeTSRange<int64_t>(range);
                        }
                        else
                        {
                            *out = core::TypedValue::makeTSTZRange<int64_t>(range);
                        }
                        return true;
                    }
                    if (field == ExtractField::LOWER_INC || field == ExtractField::UPPER_INC)
                    {
                        bool flag = false;
                        if (!toBool(new_value, flag, error))
                        {
                            return false;
                        }
                        range = core::Range<int64_t>(range.lower(), range.upper(),
                                                     field == ExtractField::LOWER_INC ? flag : range.isLowerInclusive(),
                                                     field == ExtractField::UPPER_INC ? flag : range.isUpperInclusive());
                    }
                    else if (field == ExtractField::LOWER_INF || field == ExtractField::UPPER_INF)
                    {
                        bool flag = false;
                        if (!toBool(new_value, flag, error))
                        {
                            return false;
                        }
                        std::optional<int64_t> lower = range.lower();
                        std::optional<int64_t> upper = range.upper();
                        if (field == ExtractField::LOWER_INF && flag)
                        {
                            lower.reset();
                        }
                        if (field == ExtractField::UPPER_INF && flag)
                        {
                            upper.reset();
                        }
                        range = core::Range<int64_t>(lower, upper,
                                                     range.isLowerInclusive(), range.isUpperInclusive());
                    }
                    else if (field == ExtractField::LOWER || field == ExtractField::LOWER_VALUE)
                    {
                        std::optional<int64_t> lower;
                        if (!new_value.isNull())
                        {
                            if (source_type == core::DataType::INT8RANGE)
                            {
                                lower = new_value.toInt64();
                            }
                            else if (source_type == core::DataType::DATERANGE)
                            {
                                lower = new_value.getDate();
                            }
                            else
                            {
                                lower = new_value.getTimestamp();
                            }
                        }
                        range = core::Range<int64_t>(lower, range.upper(),
                                                     range.isLowerInclusive(), range.isUpperInclusive());
                    }
                    else if (field == ExtractField::UPPER || field == ExtractField::UPPER_VALUE)
                    {
                        std::optional<int64_t> upper;
                        if (!new_value.isNull())
                        {
                            if (source_type == core::DataType::INT8RANGE)
                            {
                                upper = new_value.toInt64();
                            }
                            else if (source_type == core::DataType::DATERANGE)
                            {
                                upper = new_value.getDate();
                            }
                            else
                            {
                                upper = new_value.getTimestamp();
                            }
                        }
                        range = core::Range<int64_t>(range.lower(), upper,
                                                     range.isLowerInclusive(), range.isUpperInclusive());
                    }

                    if (source_type == core::DataType::INT8RANGE)
                    {
                        *out = core::TypedValue::makeInt8Range(range);
                    }
                    else if (source_type == core::DataType::DATERANGE)
                    {
                        *out = core::TypedValue::makeDateRange<int64_t>(range);
                    }
                    else if (source_type == core::DataType::TSRANGE)
                    {
                        *out = core::TypedValue::makeTSRange<int64_t>(range);
                    }
                    else
                    {
                        *out = core::TypedValue::makeTSTZRange<int64_t>(range);
                    }
                    return true;
                }
            }
        }

        if (source_type == core::DataType::INET)
        {
            core::InetAddr inet = source.getInet();
            if (field == ExtractField::NETMASK)
            {
                int32_t mask = new_value.toInt32();
                inet = core::InetAddr(inet.family(), inet.data(), static_cast<uint8_t>(mask));
                *out = core::TypedValue::makeInet(inet);
                return true;
            }
            if (field == ExtractField::ADDRESS)
            {
                std::string addr_text = new_value.toString();
                auto parsed = core::InetAddr::fromString(addr_text);
                if (!parsed)
                {
                    if (error) *error = "Invalid INET address";
                    return false;
                }
                inet = core::InetAddr(parsed->family(), parsed->data(), inet.netmask());
                *out = core::TypedValue::makeInet(inet);
                return true;
            }
        }

        if (source_type == core::DataType::CIDR)
        {
            core::Cidr cidr = source.getCidr();
            core::InetAddr inet = cidr.toInet();
            if (field == ExtractField::NETMASK)
            {
                int32_t mask = new_value.toInt32();
                inet = core::InetAddr(inet.family(), inet.data(), static_cast<uint8_t>(mask));
                *out = core::TypedValue::makeCidr(core::Cidr(inet));
                return true;
            }
            if (field == ExtractField::ADDRESS)
            {
                std::string addr_text = new_value.toString();
                auto parsed = core::InetAddr::fromString(addr_text);
                if (!parsed)
                {
                    if (error) *error = "Invalid CIDR address";
                    return false;
                }
                inet = core::InetAddr(parsed->family(), parsed->data(), inet.netmask());
                *out = core::TypedValue::makeCidr(core::Cidr(inet));
                return true;
            }
        }

        if (source_type == core::DataType::MACADDR || source_type == core::DataType::MACADDR8)
        {
            if (field == ExtractField::BYTES || field == ExtractField::OUI || field == ExtractField::NIC)
            {
                std::vector<uint8_t> bytes;
                if (!decodeBinaryValue(new_value, bytes, error))
                {
                    return false;
                }
                if (source_type == core::DataType::MACADDR)
                {
                    if (field == ExtractField::OUI && bytes.size() != 3)
                    {
                        if (error) *error = "OUI must be 3 bytes";
                        return false;
                    }
                    if (field == ExtractField::NIC && bytes.size() != 3)
                    {
                        if (error) *error = "NIC must be 3 bytes";
                        return false;
                    }
                    std::array<uint8_t, 6> mac = source.getMacAddr().bytes();
                    if (field == ExtractField::BYTES)
                    {
                        if (bytes.size() != 6)
                        {
                            if (error) *error = "MACADDR bytes must be 6";
                            return false;
                        }
                        std::copy(bytes.begin(), bytes.end(), mac.begin());
                    }
                    else if (field == ExtractField::OUI)
                    {
                        std::copy(bytes.begin(), bytes.end(), mac.begin());
                    }
                    else
                    {
                        std::copy(bytes.begin(), bytes.end(), mac.begin() + 3);
                    }
                    *out = core::TypedValue::makeMacAddr(core::MacAddr(mac));
                    return true;
                }
                if (field == ExtractField::OUI && bytes.size() != 3)
                {
                    if (error) *error = "OUI must be 3 bytes";
                    return false;
                }
                if (field == ExtractField::NIC && bytes.size() != 5)
                {
                    if (error) *error = "NIC must be 5 bytes";
                    return false;
                }
                std::array<uint8_t, 8> mac = source.getMacAddr8().bytes();
                if (field == ExtractField::BYTES)
                {
                    if (bytes.size() != 8)
                    {
                        if (error) *error = "MACADDR8 bytes must be 8";
                        return false;
                    }
                    std::copy(bytes.begin(), bytes.end(), mac.begin());
                }
                else if (field == ExtractField::OUI)
                {
                    std::copy(bytes.begin(), bytes.end(), mac.begin());
                }
                else
                {
                    std::copy(bytes.begin(), bytes.end(), mac.begin() + 3);
                }
                *out = core::TypedValue::makeMacAddr8(core::MacAddr8(mac));
                return true;
            }
        }

        if (source_type == core::DataType::POINT)
        {
            core::Point point = source.getPoint();
            if (new_value.isNull())
            {
                if (error) *error = "Point component cannot be NULL";
                return false;
            }
            if (field == ExtractField::X)
            {
                point.x = new_value.toDouble();
                *out = core::TypedValue::makePoint(point);
                return true;
            }
            if (field == ExtractField::Y)
            {
                point.y = new_value.toDouble();
                *out = core::TypedValue::makePoint(point);
                return true;
            }
            if (field == ExtractField::SRID)
            {
                point.srid = new_value.toInt32();
                *out = core::TypedValue::makePoint(point);
                return true;
            }
        }

        if (source_type == core::DataType::LINESTRING)
        {
            core::LineString line = source.getLineString();
            if (field == ExtractField::SRID)
            {
                line.srid = new_value.toInt32();
                *out = core::TypedValue::makeLineString(line);
                return true;
            }
        }

        if (source_type == core::DataType::POLYGON)
        {
            core::Polygon poly = source.getPolygon();
            if (field == ExtractField::SRID)
            {
                poly.srid = new_value.toInt32();
                *out = core::TypedValue::makePolygon(poly);
                return true;
            }
        }

        if (source_type == core::DataType::MULTIPOINT)
        {
            core::MultiPoint mp = source.getMultiPoint();
            if (field == ExtractField::SRID)
            {
                mp.srid = new_value.toInt32();
                *out = core::TypedValue::makeMultiPoint(mp);
                return true;
            }
        }

        if (source_type == core::DataType::MULTILINESTRING)
        {
            core::MultiLineString mls = source.getMultiLineString();
            if (field == ExtractField::SRID)
            {
                mls.srid = new_value.toInt32();
                *out = core::TypedValue::makeMultiLineString(mls);
                return true;
            }
        }

        if (source_type == core::DataType::MULTIPOLYGON)
        {
            core::MultiPolygon mp = source.getMultiPolygon();
            if (field == ExtractField::SRID)
            {
                mp.srid = new_value.toInt32();
                *out = core::TypedValue::makeMultiPolygon(mp);
                return true;
            }
        }

        if (source_type == core::DataType::GEOMETRYCOLLECTION)
        {
            core::GeometryCollection gc = source.getGeometryCollection();
            if (field == ExtractField::SRID)
            {
                gc.srid = new_value.toInt32();
                *out = core::TypedValue::makeGeometryCollection(gc);
                return true;
            }
        }

        if (error)
        {
            std::ostringstream oss;
            oss << "ALTER_ELEMENT not supported for " << static_cast<int>(source_type)
                << " field " << extractFieldToString(field);
            *error = oss.str();
        }
        return false;
    }
} // namespace scratchbird::sblr
