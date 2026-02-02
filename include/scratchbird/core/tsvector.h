/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#pragma once

#include <string>
#include <vector>
#include <algorithm>
#include <optional>
#include <cstdint>

namespace scratchbird::core
{

/**
 * Lexeme - Individual word in a text search document
 *
 * Represents a normalized word with:
 * - word: The normalized lexeme (lowercase, stemmed)
 * - positions: 1-based positions where word appears in document
 * - weights: Optional weight labels ('A', 'B', 'C', 'D') for each position
 *
 * PostgreSQL Compatibility:
 * - Positions are 1-based (PostgreSQL convention)
 * - Max position is 16383 (14 bits)
 * - Weights are stored per position, not per lexeme
 * - Lexemes are case-insensitive (normalized to lowercase)
 */
struct Lexeme
{
    std::string word;                    // Normalized word (lowercase, stemmed)
    std::vector<uint16_t> positions;     // 1-based positions (max 16383)
    std::vector<char> weights;           // Optional weights ('A', 'B', 'C', 'D'), empty = 'D'

    // Constructors
    Lexeme() = default;

    explicit Lexeme(std::string w)
        : word(std::move(w))
    {
    }

    Lexeme(std::string w, std::vector<uint16_t> pos)
        : word(std::move(w)), positions(std::move(pos))
    {
    }

    Lexeme(std::string w, std::vector<uint16_t> pos, std::vector<char> wt)
        : word(std::move(w)), positions(std::move(pos)), weights(std::move(wt))
    {
    }

    // Validation
    bool isValid() const
    {
        // Word cannot be empty
        if (word.empty())
            return false;

        // Positions and weights must match in size (if weights present)
        if (!weights.empty() && weights.size() != positions.size())
            return false;

        // Positions must be in range [1, 16383]
        for (uint16_t pos : positions)
        {
            if (pos == 0 || pos > 16383)
                return false;
        }

        // Weights must be A, B, C, or D
        for (char w : weights)
        {
            if (w != 'A' && w != 'B' && w != 'C' && w != 'D')
                return false;
        }

        // Positions should be sorted and unique
        if (!std::is_sorted(positions.begin(), positions.end()))
            return false;

        // Check for duplicates without modifying
        for (size_t i = 1; i < positions.size(); i++)
        {
            if (positions[i] == positions[i - 1])
                return false;
        }

        return true;
    }

    // Comparison operators (for sorting)
    bool operator<(const Lexeme& other) const
    {
        return word < other.word;
    }

    bool operator==(const Lexeme& other) const
    {
        return word == other.word &&
               positions == other.positions &&
               weights == other.weights;
    }

    bool operator!=(const Lexeme& other) const
    {
        return !(*this == other);
    }

    // Get weight for a specific position index
    char getWeight(size_t index) const
    {
        if (weights.empty() || index >= weights.size())
            return 'D'; // Default weight
        return weights[index];
    }

    // Add a position with optional weight
    void addPosition(uint16_t pos, char weight = 'D')
    {
        // Insert position in sorted order
        auto it = std::lower_bound(positions.begin(), positions.end(), pos);

        // Don't add duplicates
        if (it != positions.end() && *it == pos)
            return;

        size_t index = it - positions.begin();
        positions.insert(it, pos);

        // Insert weight at same index
        if (!weights.empty() || weight != 'D')
        {
            // Ensure weights vector is large enough
            if (weights.empty())
                weights.resize(positions.size() - 1, 'D');

            weights.insert(weights.begin() + index, weight);
        }
    }

    // Normalize lexeme (lowercase, trim)
    void normalize()
    {
        // Convert to lowercase
        std::transform(word.begin(), word.end(), word.begin(),
                      [](unsigned char c) { return std::tolower(c); });
    }
};

/**
 * TSVector - Text Search Vector (Document Representation)
 *
 * Represents a document as a sorted list of lexemes with positions.
 *
 * Format:
 * - Lexemes are stored in sorted order (for efficient search)
 * - Each lexeme tracks word, positions, and optional weights
 * - Positions are 1-based (PostgreSQL convention)
 *
 * String Representation:
 *   'word1':1,3 'word2':2A,5B 'word3':4
 *
 * Where:
 * - Single quotes surround each word
 * - Colon separates word from positions
 * - Comma separates positions
 * - Letter suffix indicates weight (A=highest, D=default if omitted)
 *
 * Binary Representation:
 * - Compact binary format for storage and GIN indexing
 * - Little-endian for consistency
 *
 * PostgreSQL Compatibility:
 * - Follows PostgreSQL tsvector format exactly
 * - Compatible with PostgreSQL GIN indexes
 * - Supports all PostgreSQL tsvector operations
 */
class TSVector
{
public:
    // Constructors
    TSVector() = default;

    explicit TSVector(std::vector<Lexeme> lexemes)
        : lexemes_(std::move(lexemes))
    {
        normalize();
    }

    // Factory methods

    /**
     * Parse from PostgreSQL text format: 'word1':1,3 'word2':2A,5B
     * Returns nullopt on parse error
     */
    static auto fromString(const std::string& str) -> std::optional<TSVector>;

    /**
     * Deserialize from binary format
     * Returns nullopt on invalid binary data
     */
    static auto fromBinary(const std::vector<uint8_t>& data) -> std::optional<TSVector>;
    static auto fromBinary(const uint8_t* data, size_t len) -> std::optional<TSVector>;

    // Serialization

    /**
     * Convert to PostgreSQL text format: 'word1':1,3 'word2':2A,5B
     */
    auto toString() const -> std::string;

    /**
     * Serialize to binary format (for storage and GIN indexing)
     */
    auto toBinary() const -> std::vector<uint8_t>;

    // Operations

    /**
     * Concatenate two tsvectors (merge lexemes, combine positions)
     */
    auto concat(const TSVector& other) const -> TSVector;

    /**
     * Check if tsvector contains a specific word
     */
    bool contains(const std::string& word) const;

    /**
     * Get lexeme by word (returns nullptr if not found)
     */
    auto getLexeme(const std::string& word) const -> const Lexeme*;

    /**
     * Number of unique lexemes
     */
    size_t numLexemes() const
    {
        return lexemes_.size();
    }

    /**
     * Check if empty
     */
    bool empty() const
    {
        return lexemes_.empty();
    }

    /**
     * Get all lexemes (read-only)
     */
    const std::vector<Lexeme>& lexemes() const
    {
        return lexemes_;
    }

    // Validation
    bool isValid() const;

    // Comparison operators
    bool operator==(const TSVector& other) const
    {
        return lexemes_ == other.lexemes_;
    }

    bool operator!=(const TSVector& other) const
    {
        return !(*this == other);
    }

    // Hash for GIN indexing
    size_t hash() const;

private:
    std::vector<Lexeme> lexemes_;  // Sorted by word

    // Normalize: sort lexemes, remove duplicates, merge positions
    void normalize();

    // Merge two lexemes with same word
    static auto mergeLexemes(const Lexeme& a, const Lexeme& b) -> Lexeme;
};

} // namespace scratchbird::core
