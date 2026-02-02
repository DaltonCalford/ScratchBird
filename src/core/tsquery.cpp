/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/tsquery.h"
#include <sstream>
#include <cctype>
#include <cstring>
#include <algorithm>

namespace scratchbird::core
{

// ============================================================================
// TSQueryNode Implementation
// ============================================================================

bool TSQueryNode::matches(const TSVector& vec) const
{
    switch (type_)
    {
    case Type::LEXEME:
        return matchesLexeme(vec);
    case Type::AND:
        return matchesAnd(vec);
    case Type::OR:
        return matchesOr(vec);
    case Type::NOT:
        return matchesNot(vec);
    case Type::PHRASE:
        return matchesPhrase(vec);
    }
    return false;
}

bool TSQueryNode::matchesLexeme(const TSVector& vec) const
{
    return vec.contains(term_);
}

bool TSQueryNode::matchesAnd(const TSVector& vec) const
{
    return left_->matches(vec) && right_->matches(vec);
}

bool TSQueryNode::matchesOr(const TSVector& vec) const
{
    return left_->matches(vec) || right_->matches(vec);
}

bool TSQueryNode::matchesNot(const TSVector& vec) const
{
    return !left_->matches(vec);
}

bool TSQueryNode::matchesPhrase(const TSVector& vec) const
{
    // For phrase matching, we need both terms to exist
    if (!left_->matches(vec) || !right_->matches(vec))
        return false;

    // Get both terms (must be lexemes for phrase matching)
    if (left_->type() != Type::LEXEME || right_->type() != Type::LEXEME)
        return false; // Phrase matching only works on lexemes

    const std::string& term1 = left_->term();
    const std::string& term2 = right_->term();

    // Get lexemes from tsvector
    const Lexeme* lex1 = vec.getLexeme(term1);
    const Lexeme* lex2 = vec.getLexeme(term2);

    if (!lex1 || !lex2)
        return false;

    // Check if any position in lex1 is within distance of any position in lex2
    for (uint16_t pos1 : lex1->positions)
    {
        for (uint16_t pos2 : lex2->positions)
        {
            int32_t diff = std::abs(static_cast<int32_t>(pos2) - static_cast<int32_t>(pos1));
            if (diff > 0 && static_cast<uint16_t>(diff) <= distance_)
                return true;
        }
    }

    return false;
}

auto TSQueryNode::toString() const -> std::string
{
    switch (type_)
    {
    case Type::LEXEME:
        // Escape single quotes
        {
            std::string escaped;
            for (char c : term_)
            {
                if (c == '\'')
                    escaped += "''";
                else
                    escaped += c;
            }
            return "'" + escaped + "'";
        }

    case Type::AND:
        return "(" + left_->toString() + " & " + right_->toString() + ")";

    case Type::OR:
        return "(" + left_->toString() + " | " + right_->toString() + ")";

    case Type::NOT:
        return "!" + left_->toString();

    case Type::PHRASE:
        return "(" + left_->toString() + " <" + std::to_string(distance_) + "> " +
               right_->toString() + ")";
    }

    return "";
}

bool TSQueryNode::isValid() const
{
    switch (type_)
    {
    case Type::LEXEME:
        return !term_.empty();

    case Type::AND:
    case Type::OR:
    case Type::PHRASE:
        return left_ && right_ && left_->isValid() && right_->isValid();

    case Type::NOT:
        return left_ && left_->isValid();
    }

    return false;
}

// ============================================================================
// TSQuery Implementation
// ============================================================================

auto TSQuery::fromString(const std::string& str) -> std::optional<TSQuery>
{
    if (str.empty())
        return TSQuery();

    ParseContext ctx(str);
    ctx.skipWhitespace();

    auto root = parseExpression(ctx);
    if (!root)
        return std::nullopt;

    ctx.skipWhitespace();
    if (!ctx.atEnd())
        return std::nullopt; // Unexpected characters after expression

    return TSQuery(std::move(root));
}

auto TSQuery::fromBinary(const std::vector<uint8_t>& data) -> std::optional<TSQuery>
{
    return fromBinary(data.data(), data.size());
}

auto TSQuery::fromBinary(const uint8_t* data, size_t len) -> std::optional<TSQuery>
{
    if (len == 0)
        return TSQuery();

    const uint8_t* ptr = data;
    size_t remaining = len;

    auto root = deserializeNode(ptr, remaining);
    if (!root)
        return std::nullopt;

    return TSQuery(std::move(root));
}

auto TSQuery::toString() const -> std::string
{
    if (!root_)
        return "";

    return root_->toString();
}

auto TSQuery::toBinary() const -> std::vector<uint8_t>
{
    std::vector<uint8_t> result;

    if (root_)
        serializeNode(root_.get(), result);

    return result;
}

bool TSQuery::matches(const TSVector& vec) const
{
    if (!root_)
        return false;

    return root_->matches(vec);
}

bool TSQuery::isValid() const
{
    if (!root_)
        return true; // Empty query is valid

    return root_->isValid();
}

bool TSQuery::operator==(const TSQuery& other) const
{
    return nodesEqual(root_.get(), other.root_.get());
}

size_t TSQuery::hash() const
{
    if (!root_)
        return 0;

    return hashNode(root_.get());
}

// ============================================================================
// Parser Implementation (Recursive Descent)
// ============================================================================

auto TSQuery::parseExpression(ParseContext& ctx) -> std::unique_ptr<TSQueryNode>
{
    return parseOrExpression(ctx);
}

auto TSQuery::parseOrExpression(ParseContext& ctx) -> std::unique_ptr<TSQueryNode>
{
    auto left = parseAndExpression(ctx);
    if (!left)
        return nullptr;

    ctx.skipWhitespace();

    while (ctx.current() == '|')
    {
        ctx.advance(); // Skip '|'
        ctx.skipWhitespace();

        auto right = parseAndExpression(ctx);
        if (!right)
            return nullptr;

        left = std::make_unique<TSQueryNode>(TSQueryNode::Type::OR, std::move(left),
                                            std::move(right));
        ctx.skipWhitespace();
    }

    return left;
}

auto TSQuery::parseAndExpression(ParseContext& ctx) -> std::unique_ptr<TSQueryNode>
{
    auto left = parsePhraseExpression(ctx);
    if (!left)
        return nullptr;

    ctx.skipWhitespace();

    while (ctx.current() == '&')
    {
        ctx.advance(); // Skip '&'
        ctx.skipWhitespace();

        auto right = parsePhraseExpression(ctx);
        if (!right)
            return nullptr;

        left = std::make_unique<TSQueryNode>(TSQueryNode::Type::AND, std::move(left),
                                            std::move(right));
        ctx.skipWhitespace();
    }

    return left;
}

auto TSQuery::parsePhraseExpression(ParseContext& ctx) -> std::unique_ptr<TSQueryNode>
{
    auto left = parseUnaryExpression(ctx);
    if (!left)
        return nullptr;

    ctx.skipWhitespace();

    // Check for phrase operator <N>
    if (ctx.current() == '<')
    {
        ctx.advance(); // Skip '<'

        // Read distance number
        uint32_t distance = 0;
        while (std::isdigit(ctx.current()))
        {
            distance = distance * 10 + (ctx.current() - '0');
            ctx.advance();
        }

        if (ctx.current() != '>')
            return nullptr; // Expected '>'

        ctx.advance(); // Skip '>'
        ctx.skipWhitespace();

        auto right = parseUnaryExpression(ctx);
        if (!right)
            return nullptr;

        left = std::make_unique<TSQueryNode>(TSQueryNode::Type::PHRASE, std::move(left),
                                            std::move(right), static_cast<uint16_t>(distance));
        ctx.skipWhitespace();
    }

    return left;
}

auto TSQuery::parseUnaryExpression(ParseContext& ctx) -> std::unique_ptr<TSQueryNode>
{
    ctx.skipWhitespace();

    // Check for NOT operator
    if (ctx.current() == '!')
    {
        ctx.advance(); // Skip '!'
        ctx.skipWhitespace();

        auto child = parseUnaryExpression(ctx);
        if (!child)
            return nullptr;

        return std::make_unique<TSQueryNode>(TSQueryNode::Type::NOT, std::move(child), nullptr);
    }

    return parsePrimaryExpression(ctx);
}

auto TSQuery::parsePrimaryExpression(ParseContext& ctx) -> std::unique_ptr<TSQueryNode>
{
    ctx.skipWhitespace();

    // Check for parentheses
    if (ctx.current() == '(')
    {
        ctx.advance(); // Skip '('
        ctx.skipWhitespace();

        auto expr = parseExpression(ctx);
        if (!expr)
            return nullptr;

        ctx.skipWhitespace();

        if (ctx.current() != ')')
            return nullptr; // Expected ')'

        ctx.advance(); // Skip ')'
        return expr;
    }

    // Parse term (lexeme)
    std::string term = parseTerm(ctx);
    if (term.empty())
        return nullptr;

    return std::make_unique<TSQueryNode>(TSQueryNode::Type::LEXEME, std::move(term));
}

auto TSQuery::parseTerm(ParseContext& ctx) -> std::string
{
    ctx.skipWhitespace();

    std::string term;

    // Check if term is quoted
    if (ctx.current() == '\'')
    {
        ctx.advance(); // Skip opening quote

        // Read until closing quote
        while (!ctx.atEnd() && ctx.current() != '\'')
        {
            // Handle escaped quotes ('')
            if (ctx.current() == '\'' && ctx.peek() == '\'')
            {
                term += '\'';
                ctx.advance();
                ctx.advance();
            }
            else
            {
                term += ctx.current();
                ctx.advance();
            }
        }

        if (ctx.current() != '\'')
            return ""; // Unterminated quote

        ctx.advance(); // Skip closing quote
    }
    else
    {
        // Read alphanumeric term
        while (!ctx.atEnd() && (std::isalnum(ctx.current()) || ctx.current() == '_'))
        {
            term += ctx.current();
            ctx.advance();
        }
    }

    // Normalize term (lowercase)
    std::transform(term.begin(), term.end(), term.begin(),
                  [](unsigned char c) { return std::tolower(c); });

    return term;
}

// ============================================================================
// Binary Serialization Helpers
// ============================================================================

void TSQuery::serializeNode(const TSQueryNode* node, std::vector<uint8_t>& data)
{
    if (!node)
        return;

    // Write node type (1 byte)
    data.push_back(static_cast<uint8_t>(node->type()));

    switch (node->type())
    {
    case TSQueryNode::Type::LEXEME:
        {
            // Write term length (2 bytes)
            uint16_t term_len = static_cast<uint16_t>(node->term().length());
            size_t pos = data.size();
            data.resize(pos + 2);
            std::memcpy(data.data() + pos, &term_len, 2);

            // Write term
            pos = data.size();
            data.resize(pos + term_len);
            std::memcpy(data.data() + pos, node->term().data(), term_len);
        }
        break;

    case TSQueryNode::Type::PHRASE:
        // Write distance (2 bytes)
        {
            uint16_t dist = node->distance();
            size_t pos = data.size();
            data.resize(pos + 2);
            std::memcpy(data.data() + pos, &dist, 2);
        }
        // Fall through to serialize children

    case TSQueryNode::Type::AND:
    case TSQueryNode::Type::OR:
        // Serialize both children
        serializeNode(node->left(), data);
        serializeNode(node->right(), data);
        break;

    case TSQueryNode::Type::NOT:
        // Serialize child
        serializeNode(node->left(), data);
        break;
    }
}

auto TSQuery::deserializeNode(const uint8_t*& data, size_t& remaining)
    -> std::unique_ptr<TSQueryNode>
{
    if (remaining < 1)
        return nullptr;

    // Read node type
    auto type = static_cast<TSQueryNode::Type>(*data);
    data++;
    remaining--;

    switch (type)
    {
    case TSQueryNode::Type::LEXEME:
        {
            // Read term length
            if (remaining < 2)
                return nullptr;

            uint16_t term_len;
            std::memcpy(&term_len, data, 2);
            data += 2;
            remaining -= 2;

            // Read term
            if (remaining < term_len)
                return nullptr;

            std::string term(reinterpret_cast<const char*>(data), term_len);
            data += term_len;
            remaining -= term_len;

            return std::make_unique<TSQueryNode>(type, std::move(term));
        }

    case TSQueryNode::Type::PHRASE:
        {
            // Read distance
            if (remaining < 2)
                return nullptr;

            uint16_t distance;
            std::memcpy(&distance, data, 2);
            data += 2;
            remaining -= 2;

            // Deserialize children
            auto left = deserializeNode(data, remaining);
            if (!left)
                return nullptr;

            auto right = deserializeNode(data, remaining);
            if (!right)
                return nullptr;

            return std::make_unique<TSQueryNode>(type, std::move(left), std::move(right),
                                                distance);
        }

    case TSQueryNode::Type::AND:
    case TSQueryNode::Type::OR:
        {
            // Deserialize both children
            auto left = deserializeNode(data, remaining);
            if (!left)
                return nullptr;

            auto right = deserializeNode(data, remaining);
            if (!right)
                return nullptr;

            return std::make_unique<TSQueryNode>(type, std::move(left), std::move(right));
        }

    case TSQueryNode::Type::NOT:
        {
            // Deserialize child
            auto left = deserializeNode(data, remaining);
            if (!left)
                return nullptr;

            return std::make_unique<TSQueryNode>(type, std::move(left), nullptr);
        }
    }

    return nullptr;
}

// ============================================================================
// Comparison Helpers
// ============================================================================

bool TSQuery::nodesEqual(const TSQueryNode* a, const TSQueryNode* b)
{
    if (a == nullptr && b == nullptr)
        return true;

    if (a == nullptr || b == nullptr)
        return false;

    if (a->type() != b->type())
        return false;

    switch (a->type())
    {
    case TSQueryNode::Type::LEXEME:
        return a->term() == b->term();

    case TSQueryNode::Type::PHRASE:
        if (a->distance() != b->distance())
            return false;
        // Fall through

    case TSQueryNode::Type::AND:
    case TSQueryNode::Type::OR:
        return nodesEqual(a->left(), b->left()) && nodesEqual(a->right(), b->right());

    case TSQueryNode::Type::NOT:
        return nodesEqual(a->left(), b->left());
    }

    return false;
}

size_t TSQuery::hashNode(const TSQueryNode* node)
{
    if (!node)
        return 0;

    size_t h = static_cast<size_t>(node->type());

    switch (node->type())
    {
    case TSQueryNode::Type::LEXEME:
        h ^= std::hash<std::string>{}(node->term()) + 0x9e3779b9 + (h << 6) + (h >> 2);
        break;

    case TSQueryNode::Type::PHRASE:
        h ^= node->distance() + 0x9e3779b9 + (h << 6) + (h >> 2);
        // Fall through

    case TSQueryNode::Type::AND:
    case TSQueryNode::Type::OR:
        h ^= hashNode(node->left()) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= hashNode(node->right()) + 0x9e3779b9 + (h << 6) + (h >> 2);
        break;

    case TSQueryNode::Type::NOT:
        h ^= hashNode(node->left()) + 0x9e3779b9 + (h << 6) + (h >> 2);
        break;
    }

    return h;
}

} // namespace scratchbird::core
