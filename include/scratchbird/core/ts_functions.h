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
#include "scratchbird/core/tsquery.h"
#include "scratchbird/core/ts_config.h"
#include <string>
#include <optional>

namespace scratchbird::core
{

/**
 * Text Search Conversion Functions
 *
 * PostgreSQL-compatible functions for converting text to tsvector/tsquery
 * with language-specific processing (stemming, stop words, etc.)
 */

/**
 * to_tsvector - Convert text to tsvector
 *
 * Applies language-specific text processing:
 * 1. Tokenization (split into words)
 * 2. Normalization (lowercase)
 * 3. Stop word filtering
 * 4. Stemming (morphological analysis)
 * 5. Position tracking
 *
 * @param config_name Configuration name ("simple", "english")
 * @param text Input text to process
 * @return TSVector with processed lexemes and positions
 *
 * Examples:
 *   to_tsvector("english", "The quick brown fox")
 *   -> 'brown':3 'fox':4 'quick':2
 *   (Note: "The" is filtered as stop word)
 *
 *   to_tsvector("simple", "The quick brown fox")
 *   -> 'brown':3 'fox':4 'quick':2 'the':1
 *   (Note: no stop word filtering)
 */
auto to_tsvector(const std::string& config_name, const std::string& text)
    -> std::optional<TSVector>;

/**
 * to_tsvector - Convert text to tsvector using default config
 */
auto to_tsvector(const std::string& text) -> std::optional<TSVector>;

/**
 * to_tsquery - Convert query string to tsquery
 *
 * Applies language-specific processing to query terms:
 * 1. Parse Boolean operators (&, |, !)
 * 2. Normalize terms (lowercase)
 * 3. Stem terms
 * 4. Build expression tree
 *
 * @param config_name Configuration name ("simple", "english")
 * @param query Query string with Boolean operators
 * @return TSQuery expression tree
 *
 * Examples:
 *   to_tsquery("english", "running & cats")
 *   -> 'run' & 'cat' (stemmed)
 *
 *   to_tsquery("simple", "running & cats")
 *   -> 'running' & 'cats' (no stemming)
 */
auto to_tsquery(const std::string& config_name, const std::string& query)
    -> std::optional<TSQuery>;

/**
 * to_tsquery - Convert query string using default config
 */
auto to_tsquery(const std::string& query) -> std::optional<TSQuery>;

/**
 * plainto_tsquery - Convert plain text to tsquery (implicit AND)
 *
 * Converts plain text to tsquery by:
 * 1. Tokenizing text
 * 2. Normalizing and stemming each word
 * 3. Joining with AND operators
 *
 * @param config_name Configuration name
 * @param text Plain text (no operators)
 * @return TSQuery with AND-joined terms
 *
 * Example:
 *   plainto_tsquery("english", "running cats")
 *   -> 'run' & 'cat'
 */
auto plainto_tsquery(const std::string& config_name, const std::string& text)
    -> std::optional<TSQuery>;

auto plainto_tsquery(const std::string& text) -> std::optional<TSQuery>;

/**
 * phraseto_tsquery - Convert phrase to tsquery (phrase search)
 *
 * Converts text to phrase query with <-> operators
 *
 * @param config_name Configuration name
 * @param text Phrase text
 * @return TSQuery with phrase operators
 *
 * Example:
 *   phraseto_tsquery("english", "quick brown fox")
 *   -> 'quick' <-> 'brown' <-> 'fox'
 */
auto phraseto_tsquery(const std::string& config_name, const std::string& text)
    -> std::optional<TSQuery>;

auto phraseto_tsquery(const std::string& text) -> std::optional<TSQuery>;

} // namespace scratchbird::core
