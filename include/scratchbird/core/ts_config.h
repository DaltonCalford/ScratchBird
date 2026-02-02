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
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <optional>
#include <cstdint>

namespace scratchbird::core
{

/**
 * Text Search Configuration Interface
 *
 * Defines language-specific text processing rules for full-text search:
 * - Tokenization (word boundary detection)
 * - Normalization (case folding, accent removal)
 * - Stemming (morphological analysis)
 * - Stop word filtering
 *
 * PostgreSQL Compatibility:
 * - Follows PostgreSQL text search configuration model
 * - Default configuration: 'simple' (no stemming/stop words)
 * - Language-specific configurations: 'english', 'spanish', etc.
 */
class TSConfig
{
public:
    virtual ~TSConfig() = default;

    /**
     * Get configuration name (e.g., "english", "simple")
     */
    virtual auto name() const -> std::string = 0;

    /**
     * Tokenize text into words
     * Returns list of words with positions
     */
    struct Token
    {
        std::string word;
        uint16_t position;  // 1-based position in document
    };

    virtual auto tokenize(const std::string& text) -> std::vector<Token> = 0;

    /**
     * Normalize a word (lowercase, remove accents, etc.)
     */
    virtual auto normalize(const std::string& word) -> std::string = 0;

    /**
     * Stem a word to its root form
     * Example: "running" -> "run", "cats" -> "cat"
     */
    virtual auto stem(const std::string& word) -> std::string = 0;

    /**
     * Check if word is a stop word (should be filtered out)
     * Examples: "the", "a", "an", "is", "are"
     */
    virtual auto isStopWord(const std::string& word) -> bool = 0;

    /**
     * Get registered configuration by name
     * Returns nullptr if not found
     */
    static auto get(const std::string& name) -> TSConfig*;

    /**
     * Register a configuration
     */
    static void registerConfig(std::unique_ptr<TSConfig> config);

    /**
     * Get default configuration ("simple")
     */
    static auto getDefault() -> TSConfig*;

protected:
    // Registry of configurations
    static auto getRegistry() -> std::unordered_map<std::string, std::unique_ptr<TSConfig>>&;
};

/**
 * Simple Configuration (No stemming, no stop words)
 *
 * - Tokenization: alphanumeric sequences
 * - Normalization: lowercase only
 * - Stemming: none (identity function)
 * - Stop words: none
 *
 * Use case: Non-English languages, proper nouns, technical terms
 */
class SimpleConfig : public TSConfig
{
public:
    SimpleConfig();

    auto name() const -> std::string override { return "simple"; }
    auto tokenize(const std::string& text) -> std::vector<Token> override;
    auto normalize(const std::string& word) -> std::string override;
    auto stem(const std::string& word) -> std::string override;
    auto isStopWord(const std::string& word) -> bool override;
};

/**
 * English Configuration
 *
 * - Tokenization: English word boundaries
 * - Normalization: lowercase
 * - Stemming: Porter stemmer algorithm
 * - Stop words: Common English stop words (~100 words)
 *
 * Use case: English text search
 */
class EnglishConfig : public TSConfig
{
public:
    EnglishConfig();

    auto name() const -> std::string override { return "english"; }
    auto tokenize(const std::string& text) -> std::vector<Token> override;
    auto normalize(const std::string& word) -> std::string override;
    auto stem(const std::string& word) -> std::string override;
    auto isStopWord(const std::string& word) -> bool override;

private:
    std::unordered_set<std::string> stop_words_;

    // Porter stemmer implementation
    auto porterStem(const std::string& word) -> std::string;

    // Porter stemmer helpers
    static auto isConsonant(const std::string& word, size_t i) -> bool;
    static auto measure(const std::string& word) -> int;
    static auto containsVowel(const std::string& word) -> bool;
    static auto endsWithDoubleConsonant(const std::string& word) -> bool;
    static auto endsWith(const std::string& word, const std::string& suffix) -> bool;
    static auto replaceSuffix(std::string& word, const std::string& old_suffix,
                             const std::string& new_suffix) -> bool;

    // Porter stemmer steps
    auto step1a(std::string word) -> std::string;
    auto step1b(std::string word) -> std::string;
    auto step1c(std::string word) -> std::string;
    auto step2(std::string word) -> std::string;
    auto step3(std::string word) -> std::string;
    auto step4(std::string word) -> std::string;
    auto step5a(std::string word) -> std::string;
    auto step5b(std::string word) -> std::string;
};

} // namespace scratchbird::core
