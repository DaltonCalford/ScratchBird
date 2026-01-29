#include "scratchbird/core/decfloat.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <sstream>

namespace scratchbird::core
{
    namespace
    {
        struct DecFloatLimits
        {
            int32_t max_exponent;
            int32_t min_exponent;
            int32_t min_subnormal_exponent;
            int32_t exponent_bias;
        };

        DecFloatLimits getLimits(uint8_t precision)
        {
            if (precision == 16)
            {
                return DecFloatLimits{384, -383, -398, 398};
            }
            return DecFloatLimits{6144, -6143, -6176, 6176};
        }

        uint8_t maxPrecision(uint8_t precision)
        {
            return precision == 16 ? 16 : 34;
        }

        bool isAllZero(const std::vector<uint8_t>& digits)
        {
            for (uint8_t d : digits)
            {
                if (d != 0)
                {
                    return false;
                }
            }
            return true;
        }

        void stripLeadingZeros(std::vector<uint8_t>& digits)
        {
            while (digits.size() > 1 && digits.front() == 0)
            {
                digits.erase(digits.begin());
            }
        }

        int compareDigits(const std::vector<uint8_t>& left, const std::vector<uint8_t>& right)
        {
            if (left.size() != right.size())
            {
                return left.size() < right.size() ? -1 : 1;
            }
            for (size_t i = 0; i < left.size(); ++i)
            {
                if (left[i] != right[i])
                {
                    return left[i] < right[i] ? -1 : 1;
                }
            }
            return 0;
        }

        std::vector<uint8_t> addDigits(const std::vector<uint8_t>& left,
                                       const std::vector<uint8_t>& right)
        {
            size_t max_len = std::max(left.size(), right.size());
            std::vector<uint8_t> result(max_len + 1, 0);
            int carry = 0;
            for (size_t i = 0; i < max_len; ++i)
            {
                int ld = (left.size() > i) ? left[left.size() - 1 - i] : 0;
                int rd = (right.size() > i) ? right[right.size() - 1 - i] : 0;
                int sum = ld + rd + carry;
                result[result.size() - 1 - i] = static_cast<uint8_t>(sum % 10);
                carry = sum / 10;
            }
            result[0] = static_cast<uint8_t>(carry);
            stripLeadingZeros(result);
            return result;
        }

        std::vector<uint8_t> subDigits(const std::vector<uint8_t>& left,
                                       const std::vector<uint8_t>& right)
        {
            std::vector<uint8_t> result(left.size(), 0);
            int borrow = 0;
            for (size_t i = 0; i < left.size(); ++i)
            {
                int ld = left[left.size() - 1 - i];
                int rd = (right.size() > i) ? right[right.size() - 1 - i] : 0;
                int diff = ld - rd - borrow;
                if (diff < 0)
                {
                    diff += 10;
                    borrow = 1;
                }
                else
                {
                    borrow = 0;
                }
                result[result.size() - 1 - i] = static_cast<uint8_t>(diff);
            }
            stripLeadingZeros(result);
            return result;
        }

        std::vector<uint8_t> mulDigits(const std::vector<uint8_t>& left,
                                       const std::vector<uint8_t>& right)
        {
            std::vector<uint8_t> result(left.size() + right.size(), 0);
            for (size_t i = 0; i < left.size(); ++i)
            {
                int carry = 0;
                size_t li = left.size() - 1 - i;
                for (size_t j = 0; j < right.size(); ++j)
                {
                    size_t rj = right.size() - 1 - j;
                    size_t idx = result.size() - 1 - (i + j);
                    int prod = left[li] * right[rj] + result[idx] + carry;
                    result[idx] = static_cast<uint8_t>(prod % 10);
                    carry = prod / 10;
                }
                size_t idx = result.size() - 1 - (i + right.size());
                result[idx] = static_cast<uint8_t>(result[idx] + carry);
            }
            stripLeadingZeros(result);
            return result;
        }

        void appendZeros(std::vector<uint8_t>& digits, int32_t zeros)
        {
            if (zeros <= 0)
            {
                return;
            }
            digits.insert(digits.end(), static_cast<size_t>(zeros), 0);
        }

        std::pair<std::vector<uint8_t>, std::vector<uint8_t>>
        divDigits(const std::vector<uint8_t>& numerator,
                  const std::vector<uint8_t>& denominator)
        {
            std::vector<uint8_t> quotient;
            std::vector<uint8_t> remainder;
            for (uint8_t digit : numerator)
            {
                remainder.push_back(digit);
                stripLeadingZeros(remainder);
                uint8_t qdigit = 0;
                while (compareDigits(remainder, denominator) >= 0)
                {
                    remainder = subDigits(remainder, denominator);
                    ++qdigit;
                }
                quotient.push_back(qdigit);
            }
            stripLeadingZeros(quotient);
            stripLeadingZeros(remainder);
            return {quotient, remainder};
        }

        bool hasNonZeroTail(const std::vector<uint8_t>& digits, size_t start)
        {
            for (size_t i = start; i < digits.size(); ++i)
            {
                if (digits[i] != 0)
                {
                    return true;
                }
            }
            return false;
        }

        bool shouldRoundUp(const std::vector<uint8_t>& digits, size_t cut,
                           DecFloatRoundingMode mode, bool negative)
        {
            if (cut >= digits.size())
            {
                return false;
            }
            uint8_t next_digit = digits[cut];
            bool tail_nonzero = hasNonZeroTail(digits, cut + 1);
            switch (mode)
            {
                case DecFloatRoundingMode::HALF_UP:
                    return next_digit >= 5;
                case DecFloatRoundingMode::HALF_EVEN:
                    if (next_digit > 5) return true;
                    if (next_digit < 5) return false;
                    if (tail_nonzero) return true;
                    return (cut > 0 && (digits[cut - 1] % 2 == 1));
                case DecFloatRoundingMode::DOWN:
                    return false;
                case DecFloatRoundingMode::UP:
                    return next_digit != 0 || tail_nonzero;
                case DecFloatRoundingMode::FLOOR:
                    return negative && (next_digit != 0 || tail_nonzero);
                case DecFloatRoundingMode::CEILING:
                    return !negative && (next_digit != 0 || tail_nonzero);
                default:
                    return false;
            }
        }

        bool roundDigits(std::vector<uint8_t>& digits, uint8_t precision,
                         DecFloatRoundingMode mode, bool negative)
        {
            if (digits.size() <= precision)
            {
                return false;
            }
            size_t cut = precision;
            bool inexact = hasNonZeroTail(digits, cut);
            bool round_up = shouldRoundUp(digits, cut, mode, negative);
            digits.resize(precision);
            if (round_up)
            {
                for (size_t i = 0; i < digits.size(); ++i)
                {
                    size_t idx = digits.size() - 1 - i;
                    if (digits[idx] < 9)
                    {
                        digits[idx] += 1;
                        round_up = false;
                        break;
                    }
                    digits[idx] = 0;
                }
                if (round_up)
                {
                    digits.insert(digits.begin(), 1);
                }
                inexact = true;
            }
            return inexact;
        }

        void normalizeZero(DecFloat& value)
        {
            if (value.coefficient.empty())
            {
                value.coefficient.push_back(0);
            }
            if (isAllZero(value.coefficient))
            {
                value.exponent = 0;
            }
        }

        Status applyExponentRange(DecFloat& value, const DecFloatContext& ctx,
                                  ErrorContext* err)
        {
            if (value.klass != DecFloatClass::Finite)
            {
                return Status::OK;
            }
            DecFloatLimits limits = getLimits(value.precision);
            if (isAllZero(value.coefficient))
            {
                value.exponent = 0;
                return Status::OK;
            }
            if (value.exponent > limits.max_exponent)
            {
                if (ctx.trap_overflow)
                {
                    SET_ERROR_CONTEXT(err, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                      "DECFLOAT overflow");
                    return Status::NUMERIC_VALUE_OUT_OF_RANGE;
                }
                value.klass = DecFloatClass::Infinity;
                return Status::OK;
            }
            if (value.exponent < limits.min_exponent)
            {
                int32_t shift = limits.min_exponent - value.exponent;
                if (shift >= static_cast<int32_t>(value.coefficient.size()))
                {
                    value.coefficient.assign(1, 0);
                    value.exponent = 0;
                    if (ctx.trap_underflow)
                    {
                        SET_ERROR_CONTEXT(err, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "DECFLOAT underflow");
                        return Status::NUMERIC_VALUE_OUT_OF_RANGE;
                    }
                    return Status::OK;
                }
                value.exponent = limits.min_exponent;
                if (shift > 0)
                {
                    bool inexact = roundDigits(value.coefficient,
                                               static_cast<uint8_t>(value.coefficient.size() - shift),
                                               ctx.rounding, value.negative);
                    if (inexact && ctx.trap_inexact)
                    {
                        SET_ERROR_CONTEXT(err, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "DECFLOAT inexact");
                        return Status::NUMERIC_VALUE_OUT_OF_RANGE;
                    }
                }
            }
            return Status::OK;
        }

        std::vector<uint8_t> digitsFromUInt128(unsigned __int128 value)
        {
            if (value == 0)
            {
                return {0};
            }
            std::vector<uint8_t> digits;
            while (value > 0)
            {
                uint8_t digit = static_cast<uint8_t>(value % 10);
                digits.push_back(digit);
                value /= 10;
            }
            std::reverse(digits.begin(), digits.end());
            return digits;
        }

        unsigned __int128 uint128Pow10(uint8_t power)
        {
            unsigned __int128 value = 1;
            for (uint8_t i = 0; i < power; ++i)
            {
                value *= 10;
            }
            return value;
        }

        unsigned __int128 digitsToUInt128(const std::vector<uint8_t>& digits)
        {
            unsigned __int128 value = 0;
            for (uint8_t digit : digits)
            {
                value = value * 10 + digit;
            }
            return value;
        }

        DecFloat makeNaN(uint8_t precision, bool negative)
        {
            DecFloat out;
            out.precision = maxPrecision(precision);
            out.klass = DecFloatClass::NaN;
            out.negative = negative;
            out.exponent = 0;
            out.coefficient = {0};
            return out;
        }

        DecFloat makeInfinity(uint8_t precision, bool negative)
        {
            DecFloat out;
            out.precision = maxPrecision(precision);
            out.klass = DecFloatClass::Infinity;
            out.negative = negative;
            out.exponent = 0;
            out.coefficient = {0};
            return out;
        }

        Status parseSpecial(const std::string& lower, uint8_t precision,
                            const DecFloatContext& ctx, DecFloat& out,
                            ErrorContext* err)
        {
            if (lower == "nan" || lower == "snan" ||
                lower == "inf" || lower == "infinity")
            {
                SET_ERROR_CONTEXT(err, Status::INVALID_TEXT_REPRESENTATION,
                                  "DECFLOAT special values are not enabled");
                return Status::INVALID_TEXT_REPRESENTATION;
            }
            return Status::INVALID_TEXT_REPRESENTATION;
        }

        Status finalizeValue(DecFloat& value, const DecFloatContext& ctx, ErrorContext* err)
        {
            if (value.klass != DecFloatClass::Finite)
            {
                return Status::OK;
            }
            if (value.coefficient.empty())
            {
                value.coefficient.push_back(0);
            }
            stripLeadingZeros(value.coefficient);
            if (value.coefficient.empty())
            {
                value.coefficient.push_back(0);
            }
            bool inexact = roundDigits(value.coefficient, value.precision,
                                       ctx.rounding, value.negative);
            if (value.coefficient.size() > value.precision)
            {
                value.exponent += 1;
                value.coefficient.erase(value.coefficient.begin(),
                                        value.coefficient.begin() + 1);
            }
            if (inexact && ctx.trap_inexact)
            {
                SET_ERROR_CONTEXT(err, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                  "DECFLOAT inexact");
                return Status::NUMERIC_VALUE_OUT_OF_RANGE;
            }
            normalizeZero(value);
            return applyExponentRange(value, ctx, err);
        }

        Status alignForAdd(const DecFloat& left, const DecFloat& right,
                           std::vector<uint8_t>& left_digits,
                           std::vector<uint8_t>& right_digits,
                           int32_t& exponent_out)
        {
            exponent_out = std::min(left.exponent, right.exponent);
            int32_t left_shift = left.exponent - exponent_out;
            int32_t right_shift = right.exponent - exponent_out;
            left_digits = left.coefficient;
            right_digits = right.coefficient;
            appendZeros(left_digits, left_shift);
            appendZeros(right_digits, right_shift);
            return Status::OK;
        }

        bool isFiniteNonZero(const DecFloat& value)
        {
            return value.klass == DecFloatClass::Finite && !isAllZero(value.coefficient);
        }
    }

    Status DecFloat::parse(const std::string& text, uint8_t precision,
                           const DecFloatContext& ctx, DecFloat& out,
                           ErrorContext* err)
    {
        std::string trimmed;
        for (char ch : text)
        {
            if (!std::isspace(static_cast<unsigned char>(ch)))
            {
                trimmed.push_back(ch);
            }
        }
        if (trimmed.empty())
        {
            SET_ERROR_CONTEXT(err, Status::INVALID_TEXT_REPRESENTATION,
                              "Empty DECFLOAT literal");
            return Status::INVALID_TEXT_REPRESENTATION;
        }

        bool negative = false;
        size_t pos = 0;
        if (trimmed[pos] == '+' || trimmed[pos] == '-')
        {
            negative = (trimmed[pos] == '-');
            ++pos;
        }

        std::string lower;
        lower.reserve(trimmed.size() - pos);
        for (size_t i = pos; i < trimmed.size(); ++i)
        {
            lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(trimmed[i]))));
        }
        if (lower == "nan" || lower == "snan" || lower == "inf" || lower == "infinity")
        {
            return parseSpecial(lower, precision, ctx, out, err);
        }

        std::vector<uint8_t> digits;
        int32_t exponent = 0;
        bool saw_digit = false;
        bool after_decimal = false;
        int32_t fractional_digits = 0;

        for (; pos < trimmed.size(); ++pos)
        {
            char ch = trimmed[pos];
            if (std::isdigit(static_cast<unsigned char>(ch)))
            {
                saw_digit = true;
                digits.push_back(static_cast<uint8_t>(ch - '0'));
                if (after_decimal)
                {
                    fractional_digits++;
                }
            }
            else if (ch == '.')
            {
                if (after_decimal)
                {
                    SET_ERROR_CONTEXT(err, Status::INVALID_TEXT_REPRESENTATION,
                                      "Invalid DECFLOAT literal");
                    return Status::INVALID_TEXT_REPRESENTATION;
                }
                after_decimal = true;
            }
            else if (ch == 'e' || ch == 'E')
            {
                ++pos;
                break;
            }
            else
            {
                SET_ERROR_CONTEXT(err, Status::INVALID_TEXT_REPRESENTATION,
                                  "Invalid DECFLOAT literal");
                return Status::INVALID_TEXT_REPRESENTATION;
            }
        }

        if (!saw_digit)
        {
            SET_ERROR_CONTEXT(err, Status::INVALID_TEXT_REPRESENTATION,
                              "Invalid DECFLOAT literal");
            return Status::INVALID_TEXT_REPRESENTATION;
        }

        if (pos <= trimmed.size())
        {
            if (pos < trimmed.size() && (trimmed[pos - 1] == 'e' || trimmed[pos - 1] == 'E'))
            {
                int exp_sign = 1;
                if (pos < trimmed.size() && (trimmed[pos] == '+' || trimmed[pos] == '-'))
                {
                    exp_sign = (trimmed[pos] == '-') ? -1 : 1;
                    ++pos;
                }
                if (pos >= trimmed.size())
                {
                    SET_ERROR_CONTEXT(err, Status::INVALID_TEXT_REPRESENTATION,
                                      "Invalid exponent in DECFLOAT literal");
                    return Status::INVALID_TEXT_REPRESENTATION;
                }
                int32_t exp_value = 0;
                for (; pos < trimmed.size(); ++pos)
                {
                    char ch = trimmed[pos];
                    if (!std::isdigit(static_cast<unsigned char>(ch)))
                    {
                        SET_ERROR_CONTEXT(err, Status::INVALID_TEXT_REPRESENTATION,
                                          "Invalid exponent in DECFLOAT literal");
                        return Status::INVALID_TEXT_REPRESENTATION;
                    }
                    exp_value = exp_value * 10 + (ch - '0');
                }
                exponent += exp_sign * exp_value;
            }
        }

        exponent -= fractional_digits;
        stripLeadingZeros(digits);
        if (digits.empty())
        {
            digits.push_back(0);
        }

        out.precision = maxPrecision(precision);
        out.klass = DecFloatClass::Finite;
        out.negative = negative;
        out.exponent = exponent;
        out.coefficient = digits;
        return finalizeValue(out, ctx, err);
    }

    std::string DecFloat::toString() const
    {
        if (klass == DecFloatClass::NaN)
        {
            return negative ? "-NaN" : "NaN";
        }
        if (klass == DecFloatClass::SignalingNaN)
        {
            return negative ? "-sNaN" : "sNaN";
        }
        if (klass == DecFloatClass::Infinity)
        {
            return negative ? "-Infinity" : "Infinity";
        }

        if (coefficient.empty())
        {
            return negative ? "-0" : "0";
        }

        std::ostringstream out;
        if (negative && !isAllZero(coefficient))
        {
            out << "-";
        }

        std::string digits;
        digits.reserve(coefficient.size());
        for (uint8_t d : coefficient)
        {
            digits.push_back(static_cast<char>('0' + d));
        }

        if (exponent == 0)
        {
            out << digits;
            return out.str();
        }

        if (exponent > 0)
        {
            out << digits;
            out << "E+";
            out << exponent;
            return out.str();
        }

        int32_t exp = -exponent;
        if (exp >= static_cast<int32_t>(digits.size()))
        {
            out << "0.";
            out << std::string(static_cast<size_t>(exp - digits.size()), '0');
            out << digits;
            return out.str();
        }

        size_t split = digits.size() - static_cast<size_t>(exp);
        out << digits.substr(0, split);
        out << ".";
        out << digits.substr(split);
        return out.str();
    }

    bool DecFloat::isZero() const
    {
        return klass == DecFloatClass::Finite && isAllZero(coefficient);
    }

    DecFloat DecFloat::fromBID64(uint64_t bits)
    {
        DecFloat out;
        out.precision = 16;
        out.klass = DecFloatClass::Finite;
        out.negative = (bits >> 63) != 0;

        uint64_t comb = (bits >> 58) & 0x1F;
        if (comb == 0x1E)
        {
            out.klass = DecFloatClass::Infinity;
            out.coefficient = {0};
            return out;
        }
        if (comb == 0x1F)
        {
            out.klass = DecFloatClass::NaN;
            out.coefficient = {0};
            return out;
        }

        uint64_t exp_cont = (bits >> 50) & 0xFF;
        uint64_t coeff_cont = bits & 0x3FFFFFFFFFFFFULL;
        uint64_t exp_high = 0;
        uint64_t lead = 0;
        if ((comb & 0x18) != 0x18)
        {
            exp_high = comb >> 3;
            lead = comb & 0x7;
        }
        else
        {
            exp_high = (comb >> 1) & 0x3;
            lead = 8 + (comb & 0x1);
        }

        int32_t exponent = static_cast<int32_t>((exp_high << 8) | exp_cont);
        exponent -= getLimits(16).exponent_bias;
        unsigned __int128 coeff = static_cast<unsigned __int128>(lead) * uint128Pow10(15) +
                                  static_cast<unsigned __int128>(coeff_cont);
        out.exponent = exponent;
        out.coefficient = digitsFromUInt128(coeff);
        stripLeadingZeros(out.coefficient);
        normalizeZero(out);
        return out;
    }

    DecFloat DecFloat::fromBID128(uint64_t high, uint64_t low)
    {
        DecFloat out;
        out.precision = 34;
        out.klass = DecFloatClass::Finite;
        unsigned __int128 bits = (static_cast<unsigned __int128>(high) << 64) | low;
        out.negative = (bits >> 127) != 0;

        uint64_t comb = static_cast<uint64_t>((bits >> 122) & 0x1F);
        if (comb == 0x1E)
        {
            out.klass = DecFloatClass::Infinity;
            out.coefficient = {0};
            return out;
        }
        if (comb == 0x1F)
        {
            out.klass = DecFloatClass::NaN;
            out.coefficient = {0};
            return out;
        }

        uint64_t exp_cont = static_cast<uint64_t>((bits >> 110) & 0xFFF);
        unsigned __int128 coeff_cont = bits & ((static_cast<unsigned __int128>(1) << 110) - 1);

        uint64_t exp_high = 0;
        uint64_t lead = 0;
        if ((comb & 0x18) != 0x18)
        {
            exp_high = comb >> 3;
            lead = comb & 0x7;
        }
        else
        {
            exp_high = (comb >> 1) & 0x3;
            lead = 8 + (comb & 0x1);
        }

        int32_t exponent = static_cast<int32_t>((exp_high << 12) | exp_cont);
        exponent -= getLimits(34).exponent_bias;
        unsigned __int128 coeff = static_cast<unsigned __int128>(lead) * uint128Pow10(33) +
                                  coeff_cont;
        out.exponent = exponent;
        out.coefficient = digitsFromUInt128(coeff);
        stripLeadingZeros(out.coefficient);
        normalizeZero(out);
        return out;
    }

    Status DecFloat::toBID64(uint64_t& out, const DecFloatContext& ctx,
                             ErrorContext* err) const
    {
        if (precision != 16)
        {
            SET_ERROR_CONTEXT(err, Status::INVALID_ARGUMENT,
                              "DECFLOAT16 encoding requires precision 16");
            return Status::INVALID_ARGUMENT;
        }
        if (klass == DecFloatClass::Infinity)
        {
            out = (static_cast<uint64_t>(negative) << 63) | (0x1EULL << 58);
            return Status::OK;
        }
        if (klass == DecFloatClass::NaN || klass == DecFloatClass::SignalingNaN)
        {
            out = (static_cast<uint64_t>(negative) << 63) | (0x1FULL << 58);
            return Status::OK;
        }

        DecFloat value = *this;
        Status status = finalizeValue(value, ctx, err);
        if (status != Status::OK)
        {
            return status;
        }
        DecFloatLimits limits = getLimits(16);
        if (value.exponent > limits.max_exponent || value.exponent < limits.min_subnormal_exponent)
        {
            SET_ERROR_CONTEXT(err, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                              "DECFLOAT16 exponent out of range");
            return Status::NUMERIC_VALUE_OUT_OF_RANGE;
        }

        unsigned __int128 coeff = digitsToUInt128(value.coefficient);
        unsigned __int128 pow10 = uint128Pow10(15);
        uint64_t lead = static_cast<uint64_t>(coeff / pow10);
        uint64_t coeff_cont = static_cast<uint64_t>(coeff % pow10);

        int32_t biased_exp = value.exponent + limits.exponent_bias;
        uint64_t exp_high = static_cast<uint64_t>(biased_exp >> 8) & 0x3;
        uint64_t exp_cont = static_cast<uint64_t>(biased_exp) & 0xFF;

        uint64_t comb = 0;
        if (lead <= 7)
        {
            comb = (exp_high << 3) | (lead & 0x7);
        }
        else
        {
            comb = 0x18 | (exp_high << 1) | (lead - 8);
        }

        out = (static_cast<uint64_t>(value.negative) << 63) |
              (comb << 58) |
              (exp_cont << 50) |
              (coeff_cont & 0x3FFFFFFFFFFFFULL);
        return Status::OK;
    }

    Status DecFloat::toBID128(uint64_t& high, uint64_t& low,
                              const DecFloatContext& ctx, ErrorContext* err) const
    {
        if (precision != 34)
        {
            SET_ERROR_CONTEXT(err, Status::INVALID_ARGUMENT,
                              "DECFLOAT34 encoding requires precision 34");
            return Status::INVALID_ARGUMENT;
        }
        if (klass == DecFloatClass::Infinity)
        {
            unsigned __int128 bits = (static_cast<unsigned __int128>(negative) << 127) |
                                     (static_cast<unsigned __int128>(0x1E) << 122);
            high = static_cast<uint64_t>(bits >> 64);
            low = static_cast<uint64_t>(bits);
            return Status::OK;
        }
        if (klass == DecFloatClass::NaN || klass == DecFloatClass::SignalingNaN)
        {
            unsigned __int128 bits = (static_cast<unsigned __int128>(negative) << 127) |
                                     (static_cast<unsigned __int128>(0x1F) << 122);
            high = static_cast<uint64_t>(bits >> 64);
            low = static_cast<uint64_t>(bits);
            return Status::OK;
        }

        DecFloat value = *this;
        Status status = finalizeValue(value, ctx, err);
        if (status != Status::OK)
        {
            return status;
        }
        DecFloatLimits limits = getLimits(34);
        if (value.exponent > limits.max_exponent || value.exponent < limits.min_subnormal_exponent)
        {
            SET_ERROR_CONTEXT(err, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                              "DECFLOAT34 exponent out of range");
            return Status::NUMERIC_VALUE_OUT_OF_RANGE;
        }

        unsigned __int128 coeff = digitsToUInt128(value.coefficient);
        unsigned __int128 pow10 = uint128Pow10(33);
        uint64_t lead = static_cast<uint64_t>(coeff / pow10);
        unsigned __int128 coeff_cont = coeff % pow10;

        int32_t biased_exp = value.exponent + limits.exponent_bias;
        uint64_t exp_high = static_cast<uint64_t>(biased_exp >> 12) & 0x3;
        uint64_t exp_cont = static_cast<uint64_t>(biased_exp) & 0xFFF;

        uint64_t comb = 0;
        if (lead <= 7)
        {
            comb = (exp_high << 3) | (lead & 0x7);
        }
        else
        {
            comb = 0x18 | (exp_high << 1) | (lead - 8);
        }

        unsigned __int128 bits = (static_cast<unsigned __int128>(value.negative) << 127) |
                                 (static_cast<unsigned __int128>(comb) << 122) |
                                 (static_cast<unsigned __int128>(exp_cont) << 110) |
                                 coeff_cont;

        high = static_cast<uint64_t>(bits >> 64);
        low = static_cast<uint64_t>(bits);
        return Status::OK;
    }

    Status DecFloat::add(const DecFloat& left, const DecFloat& right,
                         const DecFloatContext& ctx, DecFloat& out,
                         ErrorContext* err)
    {
        if (left.klass != DecFloatClass::Finite || right.klass != DecFloatClass::Finite)
        {
            if (left.klass == DecFloatClass::NaN || right.klass == DecFloatClass::NaN)
            {
                out = makeNaN(left.precision, false);
                return Status::OK;
            }
            if (left.klass == DecFloatClass::Infinity || right.klass == DecFloatClass::Infinity)
            {
                if (left.klass == DecFloatClass::Infinity &&
                    right.klass == DecFloatClass::Infinity &&
                    left.negative != right.negative)
                {
                    if (ctx.trap_invalid)
                    {
                        SET_ERROR_CONTEXT(err, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                          "DECFLOAT invalid operation");
                        return Status::NUMERIC_VALUE_OUT_OF_RANGE;
                    }
                    out = makeNaN(left.precision, false);
                    return Status::OK;
                }
                out = left.klass == DecFloatClass::Infinity ? left : right;
                return Status::OK;
            }
        }

        out.precision = std::max(left.precision, right.precision);
        out.klass = DecFloatClass::Finite;

        std::vector<uint8_t> left_digits;
        std::vector<uint8_t> right_digits;
        int32_t exponent = 0;
        alignForAdd(left, right, left_digits, right_digits, exponent);

        int cmp = compareDigits(left_digits, right_digits);
        if (left.negative == right.negative)
        {
            out.coefficient = addDigits(left_digits, right_digits);
            out.negative = left.negative;
        }
        else
        {
            if (cmp == 0)
            {
                out.coefficient = {0};
                out.negative = false;
                out.exponent = 0;
                return Status::OK;
            }
            if (cmp > 0)
            {
                out.coefficient = subDigits(left_digits, right_digits);
                out.negative = left.negative;
            }
            else
            {
                out.coefficient = subDigits(right_digits, left_digits);
                out.negative = right.negative;
            }
        }

        out.exponent = exponent;
        return finalizeValue(out, ctx, err);
    }

    Status DecFloat::subtract(const DecFloat& left, const DecFloat& right,
                              const DecFloatContext& ctx, DecFloat& out,
                              ErrorContext* err)
    {
        DecFloat neg_right = right;
        neg_right.negative = !right.negative;
        return add(left, neg_right, ctx, out, err);
    }

    Status DecFloat::multiply(const DecFloat& left, const DecFloat& right,
                              const DecFloatContext& ctx, DecFloat& out,
                              ErrorContext* err)
    {
        if (left.klass != DecFloatClass::Finite || right.klass != DecFloatClass::Finite)
        {
            if (left.klass == DecFloatClass::NaN || right.klass == DecFloatClass::NaN)
            {
                out = makeNaN(left.precision, false);
                return Status::OK;
            }
            if ((left.klass == DecFloatClass::Infinity && right.isZero()) ||
                (right.klass == DecFloatClass::Infinity && left.isZero()))
            {
                if (ctx.trap_invalid)
                {
                    SET_ERROR_CONTEXT(err, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                      "DECFLOAT invalid operation");
                    return Status::NUMERIC_VALUE_OUT_OF_RANGE;
                }
                out = makeNaN(left.precision, false);
                return Status::OK;
            }
            if (left.klass == DecFloatClass::Infinity || right.klass == DecFloatClass::Infinity)
            {
                out = makeInfinity(std::max(left.precision, right.precision),
                                   left.negative ^ right.negative);
                return Status::OK;
            }
        }

        out.precision = std::max(left.precision, right.precision);
        out.klass = DecFloatClass::Finite;
        out.negative = left.negative ^ right.negative;
        out.exponent = left.exponent + right.exponent;
        out.coefficient = mulDigits(left.coefficient, right.coefficient);
        return finalizeValue(out, ctx, err);
    }

    Status DecFloat::divide(const DecFloat& left, const DecFloat& right,
                            const DecFloatContext& ctx, DecFloat& out,
                            ErrorContext* err)
    {
        if (right.isZero())
        {
            if (ctx.trap_divide_by_zero)
            {
                SET_ERROR_CONTEXT(err, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                  "DECFLOAT divide by zero");
                return Status::NUMERIC_VALUE_OUT_OF_RANGE;
            }
            out = makeInfinity(std::max(left.precision, right.precision),
                               left.negative ^ right.negative);
            return Status::OK;
        }
        if (left.klass != DecFloatClass::Finite || right.klass != DecFloatClass::Finite)
        {
            if (left.klass == DecFloatClass::NaN || right.klass == DecFloatClass::NaN)
            {
                out = makeNaN(left.precision, false);
                return Status::OK;
            }
            if (left.klass == DecFloatClass::Infinity && right.klass == DecFloatClass::Infinity)
            {
                if (ctx.trap_invalid)
                {
                    SET_ERROR_CONTEXT(err, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                                      "DECFLOAT invalid operation");
                    return Status::NUMERIC_VALUE_OUT_OF_RANGE;
                }
                out = makeNaN(left.precision, false);
                return Status::OK;
            }
            if (left.klass == DecFloatClass::Infinity)
            {
                out = makeInfinity(std::max(left.precision, right.precision),
                                   left.negative ^ right.negative);
                return Status::OK;
            }
            if (right.klass == DecFloatClass::Infinity)
            {
                out = DecFloat{};
                out.precision = std::max(left.precision, right.precision);
                out.klass = DecFloatClass::Finite;
                out.negative = left.negative ^ right.negative;
                out.coefficient = {0};
                out.exponent = 0;
                return Status::OK;
            }
        }

        out.precision = std::max(left.precision, right.precision);
        out.klass = DecFloatClass::Finite;
        out.negative = left.negative ^ right.negative;

        int32_t exp = left.exponent - right.exponent;
        std::vector<uint8_t> numerator = left.coefficient;
        std::vector<uint8_t> denominator = right.coefficient;
        stripLeadingZeros(numerator);
        stripLeadingZeros(denominator);

        int32_t scale = static_cast<int32_t>(out.precision) +
                        static_cast<int32_t>(denominator.size()) -
                        static_cast<int32_t>(numerator.size());
        if (scale < 0)
        {
            scale = 0;
        }
        appendZeros(numerator, scale);
        exp -= scale;

        auto [quotient, remainder] = divDigits(numerator, denominator);
        bool inexact = !isAllZero(remainder);

        out.coefficient = quotient;
        out.exponent = exp;
        Status status = finalizeValue(out, ctx, err);
        if (status != Status::OK)
        {
            return status;
        }
        if (inexact && ctx.trap_inexact)
        {
            SET_ERROR_CONTEXT(err, Status::NUMERIC_VALUE_OUT_OF_RANGE,
                              "DECFLOAT inexact");
            return Status::NUMERIC_VALUE_OUT_OF_RANGE;
        }
        return Status::OK;
    }

    int DecFloat::compare(const DecFloat& left, const DecFloat& right, ErrorContext* err)
    {
        if (left.klass != DecFloatClass::Finite || right.klass != DecFloatClass::Finite)
        {
            if (left.klass == DecFloatClass::NaN || right.klass == DecFloatClass::NaN)
            {
                SET_ERROR_CONTEXT(err, Status::INVALID_ARGUMENT,
                                  "DECFLOAT comparison with NaN");
                return 0;
            }
            if (left.klass == DecFloatClass::Infinity && right.klass == DecFloatClass::Infinity)
            {
                if (left.negative == right.negative)
                {
                    return 0;
                }
                return left.negative ? -1 : 1;
            }
            if (left.klass == DecFloatClass::Infinity)
            {
                return left.negative ? -1 : 1;
            }
            if (right.klass == DecFloatClass::Infinity)
            {
                return right.negative ? 1 : -1;
            }
        }

        if (left.isZero() && right.isZero())
        {
            return 0;
        }
        if (left.negative != right.negative)
        {
            return left.negative ? -1 : 1;
        }

        int32_t exp = 0;
        std::vector<uint8_t> left_digits;
        std::vector<uint8_t> right_digits;
        alignForAdd(left, right, left_digits, right_digits, exp);
        int cmp = compareDigits(left_digits, right_digits);
        if (left.negative)
        {
            cmp = -cmp;
        }
        return cmp;
    }
}
