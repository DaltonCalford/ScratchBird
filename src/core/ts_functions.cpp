/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/ts_functions.h"
#include <algorithm>

namespace scratchbird::core
{

// ============================================================================
// to_tsvector Implementation
// ============================================================================

auto to_tsvector(const std::string& config_name, const std::string& text)
    -> std::optional<TSVector>
{
    // Get configuration
    TSConfig* config = TSConfig::get(config_name);
    if (!config)
        return std::nullopt;

    // Tokenize text
    auto tokens = config->tokenize(text);

    // Build lexemes
    std::vector<Lexeme> lexemes;

    for (const auto& token : tokens)
    {
        // Normalize
        std::string normalized = config->normalize(token.word);

        // Check stop words
        if (config->isStopWord(normalized))
            continue;

        // Stem
        std::string stemmed = config->stem(normalized);

        // Create lexeme
        Lexeme lex;
        lex.word = stemmed;
        lex.positions.push_back(token.position);
        lexemes.push_back(std::move(lex));
    }

    return TSVector(std::move(lexemes));
}

auto to_tsvector(const std::string& text) -> std::optional<TSVector>
{
    return to_tsvector("simple", text);
}

// ============================================================================
// to_tsquery Implementation
// ============================================================================

/**
 * Helper: Apply stemming to a TSQueryNode tree
 */
static auto stemQueryNode(const TSQueryNode* node, TSConfig* config)
    -> std::unique_ptr<TSQueryNode>
{
    if (!node)
        return nullptr;

    switch (node->type())
    {
    case TSQueryNode::Type::LEXEME:
        {
            // Stem the lexeme
            std::string normalized = config->normalize(node->term());
            std::string stemmed = config->stem(normalized);
            return std::make_unique<TSQueryNode>(TSQueryNode::Type::LEXEME, stemmed);
        }

    case TSQueryNode::Type::AND:
    case TSQueryNode::Type::OR:
        {
            auto left = stemQueryNode(node->left(), config);
            auto right = stemQueryNode(node->right(), config);
            return std::make_unique<TSQueryNode>(node->type(), std::move(left),
                                                std::move(right));
        }

    case TSQueryNode::Type::NOT:
        {
            auto left = stemQueryNode(node->left(), config);
            return std::make_unique<TSQueryNode>(node->type(), std::move(left), nullptr);
        }

    case TSQueryNode::Type::PHRASE:
        {
            auto left = stemQueryNode(node->left(), config);
            auto right = stemQueryNode(node->right(), config);
            return std::make_unique<TSQueryNode>(node->type(), std::move(left),
                                                std::move(right), node->distance());
        }
    }

    return nullptr;
}

auto to_tsquery(const std::string& config_name, const std::string& query)
    -> std::optional<TSQuery>
{
    // Get configuration
    TSConfig* config = TSConfig::get(config_name);
    if (!config)
        return std::nullopt;

    // Parse query first (without stemming)
    auto parsed = TSQuery::fromString(query);
    if (!parsed)
        return std::nullopt;

    // Apply stemming to all lexemes in the query tree
    if (parsed->root())
    {
        auto stemmed_root = stemQueryNode(parsed->root(), config);
        return TSQuery(std::move(stemmed_root));
    }

    return parsed;
}

auto to_tsquery(const std::string& query) -> std::optional<TSQuery>
{
    return to_tsquery("simple", query);
}

// ============================================================================
// plainto_tsquery Implementation
// ============================================================================

auto plainto_tsquery(const std::string& config_name, const std::string& text)
    -> std::optional<TSQuery>
{
    // Get configuration
    TSConfig* config = TSConfig::get(config_name);
    if (!config)
        return std::nullopt;

    // Tokenize and process text
    auto tokens = config->tokenize(text);

    // Build AND-connected query
    std::unique_ptr<TSQueryNode> root;

    for (const auto& token : tokens)
    {
        // Normalize and stem
        std::string normalized = config->normalize(token.word);

        // Skip stop words
        if (config->isStopWord(normalized))
            continue;

        std::string stemmed = config->stem(normalized);

        // Create lexeme node
        auto node = std::make_unique<TSQueryNode>(TSQueryNode::Type::LEXEME, stemmed);

        // Connect with AND
        if (!root)
        {
            root = std::move(node);
        }
        else
        {
            root = std::make_unique<TSQueryNode>(TSQueryNode::Type::AND,
                                                std::move(root), std::move(node));
        }
    }

    if (!root)
        return std::nullopt;

    return TSQuery(std::move(root));
}

auto plainto_tsquery(const std::string& text) -> std::optional<TSQuery>
{
    return plainto_tsquery("simple", text);
}

// ============================================================================
// phraseto_tsquery Implementation
// ============================================================================

auto phraseto_tsquery(const std::string& config_name, const std::string& text)
    -> std::optional<TSQuery>
{
    // Get configuration
    TSConfig* config = TSConfig::get(config_name);
    if (!config)
        return std::nullopt;

    // Tokenize and process text
    auto tokens = config->tokenize(text);

    // Build phrase query (adjacent terms with <1> operator)
    std::unique_ptr<TSQueryNode> root;

    for (const auto& token : tokens)
    {
        // Normalize and stem
        std::string normalized = config->normalize(token.word);

        // Skip stop words
        if (config->isStopWord(normalized))
            continue;

        std::string stemmed = config->stem(normalized);

        // Create lexeme node
        auto node = std::make_unique<TSQueryNode>(TSQueryNode::Type::LEXEME, stemmed);

        // Connect with PHRASE operator (<1> for adjacent)
        if (!root)
        {
            root = std::move(node);
        }
        else
        {
            root = std::make_unique<TSQueryNode>(TSQueryNode::Type::PHRASE,
                                                std::move(root), std::move(node),
                                                1); // distance = 1 for adjacent
        }
    }

    if (!root)
        return std::nullopt;

    return TSQuery(std::move(root));
}

auto phraseto_tsquery(const std::string& text) -> std::optional<TSQuery>
{
    return phraseto_tsquery("simple", text);
}

} // namespace scratchbird::core
