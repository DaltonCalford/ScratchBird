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

#include "scratchbird/core/tsvector.h"
#include <string>
#include <memory>
#include <vector>
#include <optional>

namespace scratchbird::core
{

/**
 * TSQueryNode - Node in a text search query expression tree
 *
 * Represents Boolean expressions over search terms:
 * - LEXEME: Leaf node containing a single search term
 * - AND: Both children must match (&)
 * - OR: Either child must match (|)
 * - NOT: Child must not match (!)
 * - PHRASE: Adjacent words in specific order (<N>)
 *
 * PostgreSQL Compatibility:
 * - Follows PostgreSQL tsquery syntax
 * - Supports & (AND), | (OR), ! (NOT), <N> (PHRASE) operators
 * - Supports parentheses for precedence
 */
class TSQueryNode
{
public:
    enum class Type
    {
        LEXEME,  // Leaf: single search term
        AND,     // Binary: left & right
        OR,      // Binary: left | right
        NOT,     // Unary: !child
        PHRASE   // Binary: left <distance> right
    };

    // Constructors
    explicit TSQueryNode(Type t) : type_(t), distance_(0) {}

    TSQueryNode(Type t, std::string term)
        : type_(t), term_(std::move(term)), distance_(0)
    {
    }

    TSQueryNode(Type t, std::unique_ptr<TSQueryNode> left, std::unique_ptr<TSQueryNode> right)
        : type_(t), left_(std::move(left)), right_(std::move(right)), distance_(0)
    {
    }

    TSQueryNode(Type t, std::unique_ptr<TSQueryNode> left, std::unique_ptr<TSQueryNode> right,
                uint16_t distance)
        : type_(t), left_(std::move(left)), right_(std::move(right)), distance_(distance)
    {
    }

    // Type accessors
    Type type() const { return type_; }
    bool isLeaf() const { return type_ == Type::LEXEME; }
    bool isOperator() const { return !isLeaf(); }

    // Term accessor (for LEXEME nodes)
    const std::string& term() const { return term_; }

    // Child accessors
    const TSQueryNode* left() const { return left_.get(); }
    const TSQueryNode* right() const { return right_.get(); }

    // Distance accessor (for PHRASE nodes)
    uint16_t distance() const { return distance_; }

    // Evaluation: check if query matches a tsvector
    bool matches(const TSVector& vec) const;

    // String representation
    auto toString() const -> std::string;

    // Validation
    bool isValid() const;

private:
    Type type_;
    std::string term_;                      // For LEXEME nodes
    std::unique_ptr<TSQueryNode> left_;     // For binary/unary operators
    std::unique_ptr<TSQueryNode> right_;    // For binary operators
    uint16_t distance_;                     // For PHRASE operator (<N>)

    // Helper for matching
    bool matchesLexeme(const TSVector& vec) const;
    bool matchesAnd(const TSVector& vec) const;
    bool matchesOr(const TSVector& vec) const;
    bool matchesNot(const TSVector& vec) const;
    bool matchesPhrase(const TSVector& vec) const;
};

/**
 * TSQuery - Text Search Query
 *
 * Represents a parsed Boolean search query with operator precedence.
 *
 * Syntax:
 * - Terms: alphanumeric words
 * - & (AND): Both terms must match (higher precedence)
 * - | (OR): Either term must match (lower precedence)
 * - ! (NOT): Term must not match (prefix operator)
 * - <N> (PHRASE): Terms within N positions (e.g., foo <2> bar)
 * - ( ): Grouping for precedence
 *
 * Examples:
 * - "cat & dog"           → documents with both "cat" and "dog"
 * - "cat | dog"           → documents with "cat" or "dog"
 * - "cat & !dog"          → documents with "cat" but not "dog"
 * - "(cat | dog) & bird"  → documents with "bird" and either "cat" or "dog"
 * - "quick <2> brown"     → documents with "quick" within 2 positions of "brown"
 *
 * PostgreSQL Compatibility:
 * - Follows PostgreSQL tsquery syntax exactly
 * - Compatible with PostgreSQL GIN indexes
 * - Supports all PostgreSQL tsquery operators
 */
class TSQuery
{
public:
    // Constructors
    TSQuery() = default;

    explicit TSQuery(std::unique_ptr<TSQueryNode> root)
        : root_(std::move(root))
    {
    }

    // Factory methods

    /**
     * Parse from PostgreSQL text format: "cat & dog | !rat"
     * Returns nullopt on parse error
     */
    static auto fromString(const std::string& str) -> std::optional<TSQuery>;

    /**
     * Deserialize from binary format
     * Returns nullopt on invalid binary data
     */
    static auto fromBinary(const std::vector<uint8_t>& data) -> std::optional<TSQuery>;
    static auto fromBinary(const uint8_t* data, size_t len) -> std::optional<TSQuery>;

    // Serialization

    /**
     * Convert to PostgreSQL text format
     */
    auto toString() const -> std::string;

    /**
     * Serialize to binary format
     */
    auto toBinary() const -> std::vector<uint8_t>;

    // Evaluation

    /**
     * Check if query matches a tsvector
     */
    bool matches(const TSVector& vec) const;

    // Properties

    /**
     * Check if empty (no query)
     */
    bool empty() const
    {
        return root_ == nullptr;
    }

    /**
     * Get root node (read-only)
     */
    const TSQueryNode* root() const
    {
        return root_.get();
    }

    // Validation
    bool isValid() const;

    // Comparison operators
    bool operator==(const TSQuery& other) const;
    bool operator!=(const TSQuery& other) const
    {
        return !(*this == other);
    }

    // Hash for GIN indexing
    size_t hash() const;

private:
    std::unique_ptr<TSQueryNode> root_;

    // Parser helpers
    struct ParseContext
    {
        const char* input;
        size_t pos;
        size_t len;

        ParseContext(const std::string& str)
            : input(str.c_str()), pos(0), len(str.length())
        {
        }

        char current() const
        {
            return pos < len ? input[pos] : '\0';
        }

        char peek(size_t offset = 1) const
        {
            return (pos + offset < len) ? input[pos + offset] : '\0';
        }

        void advance()
        {
            if (pos < len)
                pos++;
        }

        void skipWhitespace()
        {
            while (pos < len && std::isspace(current()))
                pos++;
        }

        bool atEnd() const
        {
            return pos >= len;
        }
    };

    // Recursive descent parser
    static auto parseExpression(ParseContext& ctx) -> std::unique_ptr<TSQueryNode>;
    static auto parseOrExpression(ParseContext& ctx) -> std::unique_ptr<TSQueryNode>;
    static auto parseAndExpression(ParseContext& ctx) -> std::unique_ptr<TSQueryNode>;
    static auto parsePhraseExpression(ParseContext& ctx) -> std::unique_ptr<TSQueryNode>;
    static auto parseUnaryExpression(ParseContext& ctx) -> std::unique_ptr<TSQueryNode>;
    static auto parsePrimaryExpression(ParseContext& ctx) -> std::unique_ptr<TSQueryNode>;
    static auto parseTerm(ParseContext& ctx) -> std::string;

    // Binary serialization helpers
    static void serializeNode(const TSQueryNode* node, std::vector<uint8_t>& data);
    static auto deserializeNode(const uint8_t*& data, size_t& remaining)
        -> std::unique_ptr<TSQueryNode>;

    // Comparison helpers
    static bool nodesEqual(const TSQueryNode* a, const TSQueryNode* b);
    static size_t hashNode(const TSQueryNode* node);
};

} // namespace scratchbird::core
