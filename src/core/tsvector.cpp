/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/tsvector.h"
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace scratchbird::core
{

// ============================================================================
// TSVector Implementation
// ============================================================================

auto TSVector::fromString(const std::string& str) -> std::optional<TSVector>
{
    if (str.empty())
        return TSVector();

    std::vector<Lexeme> lexemes;
    size_t pos = 0;

    // Skip leading whitespace
    while (pos < str.length() && std::isspace(str[pos]))
        pos++;

    while (pos < str.length())
    {
        // Expect single quote
        if (str[pos] != '\'')
            return std::nullopt;
        pos++;

        // Read word until closing quote
        std::string word;
        while (pos < str.length() && str[pos] != '\'')
        {
            // Handle escaped single quotes ('')
            if (str[pos] == '\'' && pos + 1 < str.length() && str[pos + 1] == '\'')
            {
                word += '\'';
                pos += 2;
            }
            else
            {
                word += str[pos];
                pos++;
            }
        }

        if (pos >= str.length())
            return std::nullopt; // Unterminated quote

        pos++; // Skip closing quote

        // Word cannot be empty
        if (word.empty())
            return std::nullopt;

        Lexeme lexeme(word);

        // Check for positions
        if (pos < str.length() && str[pos] == ':')
        {
            pos++; // Skip colon

            // Read positions
            while (pos < str.length() && (std::isdigit(str[pos]) || str[pos] == ','))
            {
                // Read position number
                uint32_t position = 0;
                while (pos < str.length() && std::isdigit(str[pos]))
                {
                    position = position * 10 + (str[pos] - '0');
                    pos++;
                }

                if (position == 0 || position > 16383)
                    return std::nullopt; // Invalid position

                // Check for weight suffix
                char weight = 'D'; // Default weight
                if (pos < str.length() && (str[pos] == 'A' || str[pos] == 'B' ||
                                          str[pos] == 'C' || str[pos] == 'D'))
                {
                    weight = str[pos];
                    pos++;
                }

                lexeme.addPosition(static_cast<uint16_t>(position), weight);

                // Skip comma
                if (pos < str.length() && str[pos] == ',')
                    pos++;
            }
        }

        lexemes.push_back(std::move(lexeme));

        // Skip whitespace between lexemes
        while (pos < str.length() && std::isspace(str[pos]))
            pos++;
    }

    return TSVector(std::move(lexemes));
}

auto TSVector::fromBinary(const std::vector<uint8_t>& data) -> std::optional<TSVector>
{
    return fromBinary(data.data(), data.size());
}

auto TSVector::fromBinary(const uint8_t* data, size_t len) -> std::optional<TSVector>
{
    if (len < 4)
        return std::nullopt;

    size_t pos = 0;

    // Read number of lexemes (4 bytes, little-endian)
    uint32_t num_lexemes;
    std::memcpy(&num_lexemes, data + pos, 4);
    pos += 4;

    std::vector<Lexeme> lexemes;
    lexemes.reserve(num_lexemes);

    for (uint32_t i = 0; i < num_lexemes; i++)
    {
        // Read word length (2 bytes)
        if (pos + 2 > len)
            return std::nullopt;

        uint16_t word_len;
        std::memcpy(&word_len, data + pos, 2);
        pos += 2;

        // Read word
        if (pos + word_len > len)
            return std::nullopt;

        std::string word(reinterpret_cast<const char*>(data + pos), word_len);
        pos += word_len;

        // Read number of positions (2 bytes)
        if (pos + 2 > len)
            return std::nullopt;

        uint16_t num_positions;
        std::memcpy(&num_positions, data + pos, 2);
        pos += 2;

        std::vector<uint16_t> positions;
        std::vector<char> weights;

        // Read positions and weights
        for (uint16_t j = 0; j < num_positions; j++)
        {
            // Position (2 bytes)
            if (pos + 2 > len)
                return std::nullopt;

            uint16_t position;
            std::memcpy(&position, data + pos, 2);
            pos += 2;

            positions.push_back(position);

            // Weight (1 byte)
            if (pos + 1 > len)
                return std::nullopt;

            char weight = static_cast<char>(data[pos]);
            pos++;

            // Only store weight if not default 'D'
            if (weight != 'D')
            {
                // Ensure weights vector is sized correctly
                if (weights.empty())
                    weights.resize(j, 'D');
                weights.push_back(weight);
            }
            else if (!weights.empty())
            {
                weights.push_back('D');
            }
        }

        lexemes.emplace_back(std::move(word), std::move(positions), std::move(weights));
    }

    return TSVector(std::move(lexemes));
}

auto TSVector::toString() const -> std::string
{
    if (lexemes_.empty())
        return "";

    std::ostringstream oss;

    for (size_t i = 0; i < lexemes_.size(); i++)
    {
        const Lexeme& lex = lexemes_[i];

        if (i > 0)
            oss << ' ';

        // Quote word (escape single quotes)
        oss << '\'';
        for (char c : lex.word)
        {
            if (c == '\'')
                oss << "''"; // Escape single quote
            else
                oss << c;
        }
        oss << '\'';

        // Write positions
        if (!lex.positions.empty())
        {
            oss << ':';
            for (size_t j = 0; j < lex.positions.size(); j++)
            {
                if (j > 0)
                    oss << ',';

                oss << lex.positions[j];

                // Add weight suffix if not default 'D'
                char weight = lex.getWeight(j);
                if (weight != 'D')
                    oss << weight;
            }
        }
    }

    return oss.str();
}

auto TSVector::toBinary() const -> std::vector<uint8_t>
{
    std::vector<uint8_t> result;

    // Write number of lexemes (4 bytes)
    uint32_t num_lexemes = static_cast<uint32_t>(lexemes_.size());
    result.resize(4);
    std::memcpy(result.data(), &num_lexemes, 4);

    for (const Lexeme& lex : lexemes_)
    {
        // Write word length (2 bytes)
        uint16_t word_len = static_cast<uint16_t>(lex.word.length());
        size_t pos = result.size();
        result.resize(pos + 2);
        std::memcpy(result.data() + pos, &word_len, 2);

        // Write word
        pos = result.size();
        result.resize(pos + word_len);
        std::memcpy(result.data() + pos, lex.word.data(), word_len);

        // Write number of positions (2 bytes)
        uint16_t num_positions = static_cast<uint16_t>(lex.positions.size());
        pos = result.size();
        result.resize(pos + 2);
        std::memcpy(result.data() + pos, &num_positions, 2);

        // Write positions and weights
        for (size_t i = 0; i < lex.positions.size(); i++)
        {
            // Position (2 bytes)
            pos = result.size();
            result.resize(pos + 2);
            std::memcpy(result.data() + pos, &lex.positions[i], 2);

            // Weight (1 byte)
            char weight = lex.getWeight(i);
            result.push_back(static_cast<uint8_t>(weight));
        }
    }

    return result;
}

auto TSVector::concat(const TSVector& other) const -> TSVector
{
    std::vector<Lexeme> merged = lexemes_;
    merged.insert(merged.end(), other.lexemes_.begin(), other.lexemes_.end());
    return TSVector(std::move(merged));
}

bool TSVector::contains(const std::string& word) const
{
    // Binary search (lexemes are sorted)
    std::string normalized_word = word;
    std::transform(normalized_word.begin(), normalized_word.end(), normalized_word.begin(),
                  [](unsigned char c) { return std::tolower(c); });

    auto it = std::lower_bound(lexemes_.begin(), lexemes_.end(), normalized_word,
                              [](const Lexeme& lex, const std::string& w) {
                                  return lex.word < w;
                              });

    return it != lexemes_.end() && it->word == normalized_word;
}

auto TSVector::getLexeme(const std::string& word) const -> const Lexeme*
{
    std::string normalized_word = word;
    std::transform(normalized_word.begin(), normalized_word.end(), normalized_word.begin(),
                  [](unsigned char c) { return std::tolower(c); });

    auto it = std::lower_bound(lexemes_.begin(), lexemes_.end(), normalized_word,
                              [](const Lexeme& lex, const std::string& w) {
                                  return lex.word < w;
                              });

    if (it != lexemes_.end() && it->word == normalized_word)
        return &(*it);

    return nullptr;
}

bool TSVector::isValid() const
{
    // Check all lexemes are valid
    for (const Lexeme& lex : lexemes_)
    {
        if (!lex.isValid())
            return false;
    }

    // Check lexemes are sorted
    if (!std::is_sorted(lexemes_.begin(), lexemes_.end(),
                       [](const Lexeme& a, const Lexeme& b) {
                           return a.word < b.word;
                       }))
    {
        return false;
    }

    // Check no duplicate words
    for (size_t i = 1; i < lexemes_.size(); i++)
    {
        if (lexemes_[i - 1].word == lexemes_[i].word)
            return false;
    }

    return true;
}

size_t TSVector::hash() const
{
    size_t h = 0;
    for (const Lexeme& lex : lexemes_)
    {
        // Simple hash combining word hashes
        h ^= std::hash<std::string>{}(lex.word) + 0x9e3779b9 + (h << 6) + (h >> 2);
    }
    return h;
}

void TSVector::normalize()
{
    // Normalize all lexeme words
    for (Lexeme& lex : lexemes_)
    {
        lex.normalize();
    }

    // Sort lexemes by word
    std::sort(lexemes_.begin(), lexemes_.end(),
             [](const Lexeme& a, const Lexeme& b) {
                 return a.word < b.word;
             });

    // Merge duplicate words
    std::vector<Lexeme> merged;
    for (size_t i = 0; i < lexemes_.size(); i++)
    {
        if (merged.empty() || merged.back().word != lexemes_[i].word)
        {
            merged.push_back(std::move(lexemes_[i]));
        }
        else
        {
            // Merge with previous lexeme
            merged.back() = mergeLexemes(merged.back(), lexemes_[i]);
        }
    }

    lexemes_ = std::move(merged);
}

auto TSVector::mergeLexemes(const Lexeme& a, const Lexeme& b) -> Lexeme
{
    Lexeme result;
    result.word = a.word;

    // Merge positions (sorted union)
    size_t i = 0, j = 0;
    while (i < a.positions.size() && j < b.positions.size())
    {
        if (a.positions[i] < b.positions[j])
        {
            result.positions.push_back(a.positions[i]);
            if (!a.weights.empty())
                result.weights.push_back(a.getWeight(i));
            i++;
        }
        else if (a.positions[i] > b.positions[j])
        {
            result.positions.push_back(b.positions[j]);
            if (!b.weights.empty())
                result.weights.push_back(b.getWeight(j));
            j++;
        }
        else
        {
            // Same position - keep higher weight (A > B > C > D)
            result.positions.push_back(a.positions[i]);
            char weight_a = a.getWeight(i);
            char weight_b = b.getWeight(j);
            char weight = (weight_a < weight_b) ? weight_a : weight_b; // 'A' < 'B' < 'C' < 'D'
            if (weight != 'D' || !result.weights.empty())
                result.weights.push_back(weight);
            i++;
            j++;
        }
    }

    // Append remaining positions from a
    while (i < a.positions.size())
    {
        result.positions.push_back(a.positions[i]);
        if (!a.weights.empty())
            result.weights.push_back(a.getWeight(i));
        i++;
    }

    // Append remaining positions from b
    while (j < b.positions.size())
    {
        result.positions.push_back(b.positions[j]);
        if (!b.weights.empty())
            result.weights.push_back(b.getWeight(j));
        j++;
    }

    return result;
}

} // namespace scratchbird::core
