#include "scratchbird/core/data_masking.h"
#include "scratchbird/core/utf8_utils.h"
#include <algorithm>

namespace scratchbird::core
{
    namespace
    {
        Status normalizeMaskChar(const std::string& mask_char,
                                 std::string& mask_out,
                                 ErrorContext* ctx)
        {
            if (mask_char.empty())
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Mask character cannot be empty");
                return Status::INVALID_ARGUMENT;
            }
            mask_out = mask_char;
            return Status::OK;
        }

        std::vector<std::string> splitUtf8OrBytes(const std::string& value)
        {
            bool utf8_valid = UTF8Utils::isValidUTF8(value);
            std::vector<std::string> chars;
            if (!utf8_valid)
            {
                chars.reserve(value.size());
                for (char ch : value)
                {
                    chars.emplace_back(1, ch);
                }
                return chars;
            }

            size_t pos = 0;
            while (pos < value.size())
            {
                size_t start = pos;
                auto decoded = UTF8Utils::decodeChar(value, pos);
                if (!decoded.has_value())
                {
                    utf8_valid = false;
                    chars.clear();
                    chars.reserve(value.size());
                    for (char ch : value)
                    {
                        chars.emplace_back(1, ch);
                    }
                    return chars;
                }
                chars.emplace_back(value.substr(start, pos - start));
            }
            return chars;
        }
    }

    Status DataMasking::applyMasking(const std::string& value,
                                     const MaskingConfig& config,
                                     bool has_privilege,
                                     std::string& masked_out,
                                     ErrorContext* ctx)
    {
        if (has_privilege || config.type == MaskingType::NONE)
        {
            masked_out = value;
            return Status::OK;
        }

        switch (config.type)
        {
        case MaskingType::FULL:
            return applyFullMasking(value, config.full_mask_char, masked_out, ctx);
        case MaskingType::PARTIAL:
            return applyPartialMasking(value, config.pattern, config.full_mask_char, masked_out, ctx);
        case MaskingType::NONE:
            masked_out = value;
            return Status::OK;
        }

        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Unknown masking type");
        return Status::INVALID_ARGUMENT;
    }

    Status DataMasking::parsePattern(const std::string& pattern,
                                     std::vector<char>& parsed_out,
                                     ErrorContext* ctx)
    {
        if (pattern.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Masking pattern cannot be empty");
            return Status::INVALID_ARGUMENT;
        }

        parsed_out.assign(pattern.begin(), pattern.end());
        return Status::OK;
    }

    Status DataMasking::applyPartialMasking(const std::string& value,
                                           const std::string& pattern,
                                           const std::string& mask_char,
                                           std::string& masked_out,
                                           ErrorContext* ctx)
    {
        if (pattern.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Masking pattern cannot be empty");
            return Status::INVALID_ARGUMENT;
        }

        std::string mask_token;
        Status status = normalizeMaskChar(mask_char, mask_token, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        std::vector<std::string> value_chars = splitUtf8OrBytes(value);

        std::string output;
        output.reserve(pattern.size() + value.size());

        size_t input_idx = 0;
        for (char token : pattern)
        {
            if (token == '#')
            {
                if (input_idx < value_chars.size())
                {
                    output += value_chars[input_idx++];
                }
                else
                {
                    output += mask_token;
                }
            }
            else if (token == 'X')
            {
                if (input_idx < value_chars.size())
                {
                    ++input_idx;
                }
                output += mask_token;
            }
            else
            {
                output.push_back(token);
                if (input_idx < value_chars.size() &&
                    value_chars[input_idx].size() == 1 &&
                    value_chars[input_idx][0] == token)
                {
                    ++input_idx;
                }
            }
        }

        for (; input_idx < value_chars.size(); ++input_idx)
        {
            output += mask_token;
        }

        masked_out = std::move(output);
        return Status::OK;
    }

    Status DataMasking::applyFullMasking(const std::string& value,
                                         const std::string& mask_char,
                                         std::string& masked_out,
                                         ErrorContext* ctx)
    {
        std::string mask_token;
        Status status = normalizeMaskChar(mask_char, mask_token, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        size_t char_count = UTF8Utils::isValidUTF8(value)
            ? UTF8Utils::countCharacters(value)
            : value.size();

        masked_out.clear();
        masked_out.reserve(char_count * mask_token.size());
        for (size_t i = 0; i < char_count; ++i)
        {
            masked_out += mask_token;
        }

        return Status::OK;
    }
} // namespace scratchbird::core
