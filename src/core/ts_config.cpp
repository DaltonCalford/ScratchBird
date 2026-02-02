/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/ts_config.h"
#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace scratchbird::core
{

// ============================================================================
// TSConfig Static Registry
// ============================================================================

auto TSConfig::getRegistry() -> std::unordered_map<std::string, std::unique_ptr<TSConfig>>&
{
    static std::unordered_map<std::string, std::unique_ptr<TSConfig>> registry;
    static bool initialized = false;

    if (!initialized)
    {
        // Register default configurations
        registry["simple"] = std::make_unique<SimpleConfig>();
        registry["english"] = std::make_unique<EnglishConfig>();
        initialized = true;
    }

    return registry;
}

auto TSConfig::get(const std::string& name) -> TSConfig*
{
    auto& registry = getRegistry();
    auto it = registry.find(name);
    return (it != registry.end()) ? it->second.get() : nullptr;
}

void TSConfig::registerConfig(std::unique_ptr<TSConfig> config)
{
    auto& registry = getRegistry();
    std::string config_name = config->name();
    registry[config_name] = std::move(config);
}

auto TSConfig::getDefault() -> TSConfig*
{
    return get("simple");
}

// ============================================================================
// SimpleConfig Implementation
// ============================================================================

SimpleConfig::SimpleConfig()
{
}

auto SimpleConfig::tokenize(const std::string& text) -> std::vector<Token>
{
    std::vector<Token> tokens;
    std::string current_word;
    uint16_t position = 1;

    for (size_t i = 0; i < text.length(); i++)
    {
        char c = text[i];

        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_')
        {
            current_word += c;
        }
        else
        {
            if (!current_word.empty())
            {
                tokens.push_back({current_word, position});
                position++;
                current_word.clear();
            }
        }
    }

    // Add final word
    if (!current_word.empty())
    {
        tokens.push_back({current_word, position});
    }

    return tokens;
}

auto SimpleConfig::normalize(const std::string& word) -> std::string
{
    std::string normalized = word;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return normalized;
}

auto SimpleConfig::stem(const std::string& word) -> std::string
{
    // No stemming in simple config
    return word;
}

auto SimpleConfig::isStopWord(const std::string& word) -> bool
{
    // No stop words in simple config
    (void)word;
    return false;
}

// ============================================================================
// EnglishConfig Implementation
// ============================================================================

EnglishConfig::EnglishConfig()
{
    // Common English stop words (from PostgreSQL english.stop)
    stop_words_ = {
        "a", "am", "an", "and", "are", "as", "at", "be", "but", "by",
        "for", "if", "in", "into", "is", "it",
        "no", "not", "of", "on", "or", "such",
        "that", "the", "their", "then", "there", "these",
        "they", "this", "to", "was", "will", "with"
    };
}

auto EnglishConfig::tokenize(const std::string& text) -> std::vector<Token>
{
    // Same tokenization as SimpleConfig for now
    // Could be enhanced with English-specific rules
    SimpleConfig simple;
    return simple.tokenize(text);
}

auto EnglishConfig::normalize(const std::string& word) -> std::string
{
    std::string normalized = word;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    return normalized;
}

auto EnglishConfig::stem(const std::string& word) -> std::string
{
    return porterStem(normalize(word));
}

auto EnglishConfig::isStopWord(const std::string& word) -> bool
{
    std::string normalized_word = normalize(word);
    return stop_words_.find(normalized_word) != stop_words_.end();
}

// ============================================================================
// Porter Stemmer Implementation
// ============================================================================

auto EnglishConfig::isConsonant(const std::string& word, size_t i) -> bool
{
    if (i >= word.length())
        return false;

    char c = word[i];

    // Vowels: a, e, i, o, u
    if (c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u')
        return false;

    // 'y' is consonant if preceded by vowel
    if (c == 'y')
    {
        if (i == 0)
            return true;
        return !isConsonant(word, i - 1);
    }

    return true;
}

auto EnglishConfig::measure(const std::string& word) -> int
{
    int m = 0;
    size_t i = 0;

    // Skip initial consonants
    while (i < word.length() && isConsonant(word, i))
        i++;

    // Count VC sequences
    while (i < word.length())
    {
        // Skip vowels
        while (i < word.length() && !isConsonant(word, i))
            i++;

        if (i >= word.length())
            break;

        m++;

        // Skip consonants
        while (i < word.length() && isConsonant(word, i))
            i++;
    }

    return m;
}

auto EnglishConfig::containsVowel(const std::string& word) -> bool
{
    for (size_t i = 0; i < word.length(); i++)
    {
        if (!isConsonant(word, i))
            return true;
    }
    return false;
}

auto EnglishConfig::endsWithDoubleConsonant(const std::string& word) -> bool
{
    if (word.length() < 2)
        return false;

    size_t i = word.length() - 1;
    return word[i] == word[i - 1] && isConsonant(word, i);
}

auto EnglishConfig::endsWith(const std::string& word, const std::string& suffix) -> bool
{
    if (word.length() < suffix.length())
        return false;

    return word.substr(word.length() - suffix.length()) == suffix;
}

auto EnglishConfig::replaceSuffix(std::string& word, const std::string& old_suffix,
                                  const std::string& new_suffix) -> bool
{
    if (!endsWith(word, old_suffix))
        return false;

    word = word.substr(0, word.length() - old_suffix.length()) + new_suffix;
    return true;
}

auto EnglishConfig::porterStem(const std::string& word) -> std::string
{
    if (word.length() <= 2)
        return word;

    std::string result = word;

    // Apply Porter stemmer steps
    result = step1a(result);
    result = step1b(result);
    result = step1c(result);
    result = step2(result);
    result = step3(result);
    result = step4(result);
    result = step5a(result);
    result = step5b(result);

    return result;
}

auto EnglishConfig::step1a(std::string word) -> std::string
{
    // SSES -> SS (caresses -> caress)
    if (endsWith(word, "sses"))
    {
        word = word.substr(0, word.length() - 2);
        return word;
    }

    // IES -> I (ponies -> poni, ties -> ti)
    if (endsWith(word, "ies"))
    {
        word = word.substr(0, word.length() - 2);
        return word;
    }

    // SS -> SS (caress -> caress)
    if (endsWith(word, "ss"))
    {
        return word;
    }

    // S -> (cats -> cat)
    if (endsWith(word, "s"))
    {
        word = word.substr(0, word.length() - 1);
        return word;
    }

    return word;
}

auto EnglishConfig::step1b(std::string word) -> std::string
{
    bool second_step = false;

    // (m>0) EED -> EE (agreed -> agree)
    if (endsWith(word, "eed"))
    {
        std::string stem = word.substr(0, word.length() - 3);
        if (measure(stem) > 0)
        {
            word = word.substr(0, word.length() - 1);
        }
        return word;
    }

    // (*v*) ED -> (plastered -> plaster)
    if (endsWith(word, "ed"))
    {
        std::string stem = word.substr(0, word.length() - 2);
        if (containsVowel(stem))
        {
            word = stem;
            second_step = true;
        }
    }
    // (*v*) ING -> (motoring -> motor)
    else if (endsWith(word, "ing"))
    {
        std::string stem = word.substr(0, word.length() - 3);
        if (containsVowel(stem))
        {
            word = stem;
            second_step = true;
        }
    }

    if (second_step)
    {
        // AT -> ATE, BL -> BLE, IZ -> IZE
        if (endsWith(word, "at") || endsWith(word, "bl") || endsWith(word, "iz"))
        {
            word += "e";
        }
        // Double consonant (not l, s, z) -> single letter
        else if (endsWithDoubleConsonant(word))
        {
            char last = word[word.length() - 1];
            if (last != 'l' && last != 's' && last != 'z')
            {
                word = word.substr(0, word.length() - 1);
            }
        }
        // (m=1 and *o) -> E
        else if (measure(word) == 1)
        {
            // Check for CVC pattern at end (consonant-vowel-consonant)
            if (word.length() >= 3)
            {
                size_t len = word.length();
                if (isConsonant(word, len - 1) && !isConsonant(word, len - 2) &&
                    isConsonant(word, len - 3))
                {
                    char last = word[len - 1];
                    if (last != 'w' && last != 'x' && last != 'y')
                    {
                        word += "e";
                    }
                }
            }
        }
    }

    return word;
}

auto EnglishConfig::step1c(std::string word) -> std::string
{
    // (*v*) Y -> I (happy -> happi)
    if (endsWith(word, "y"))
    {
        std::string stem = word.substr(0, word.length() - 1);
        if (containsVowel(stem))
        {
            word = stem + "i";
        }
    }

    return word;
}

auto EnglishConfig::step2(std::string word) -> std::string
{
    // (m>0) suffix -> new_suffix
    struct Replacement
    {
        std::string old_suffix;
        std::string new_suffix;
    };

    std::vector<Replacement> replacements = {
        {"ational", "ate"}, {"tional", "tion"}, {"enci", "ence"}, {"anci", "ance"},
        {"izer", "ize"},    {"abli", "able"},   {"alli", "al"},   {"entli", "ent"},
        {"eli", "e"},       {"ousli", "ous"},   {"ization", "ize"}, {"ation", "ate"},
        {"ator", "ate"},    {"alism", "al"},    {"iveness", "ive"}, {"fulness", "ful"},
        {"ousness", "ous"}, {"aliti", "al"},    {"iviti", "ive"},   {"biliti", "ble"}
    };

    for (const auto& repl : replacements)
    {
        if (endsWith(word, repl.old_suffix))
        {
            std::string stem = word.substr(0, word.length() - repl.old_suffix.length());
            if (measure(stem) > 0)
            {
                word = stem + repl.new_suffix;
            }
            break;
        }
    }

    return word;
}

auto EnglishConfig::step3(std::string word) -> std::string
{
    // (m>0) suffix -> new_suffix
    struct Replacement
    {
        std::string old_suffix;
        std::string new_suffix;
    };

    std::vector<Replacement> replacements = {
        {"icate", "ic"}, {"ative", ""},   {"alize", "al"},
        {"iciti", "ic"}, {"ical", "ic"},  {"ful", ""},
        {"ness", ""}
    };

    for (const auto& repl : replacements)
    {
        if (endsWith(word, repl.old_suffix))
        {
            std::string stem = word.substr(0, word.length() - repl.old_suffix.length());
            if (measure(stem) > 0)
            {
                word = stem + repl.new_suffix;
            }
            break;
        }
    }

    return word;
}

auto EnglishConfig::step4(std::string word) -> std::string
{
    // (m>1) suffix -> ""
    std::vector<std::string> suffixes = {
        "al", "ance", "ence", "er", "ic", "able", "ible", "ant",
        "ement", "ment", "ent", "ion", "ou", "ism", "ate", "iti",
        "ous", "ive", "ize"
    };

    for (const auto& suffix : suffixes)
    {
        if (endsWith(word, suffix))
        {
            std::string stem = word.substr(0, word.length() - suffix.length());

            // Special case for "ion"
            if (suffix == "ion")
            {
                if (stem.length() > 0)
                {
                    char last = stem[stem.length() - 1];
                    if ((last == 's' || last == 't') && measure(stem) > 1)
                    {
                        word = stem;
                    }
                }
            }
            else if (measure(stem) > 1)
            {
                word = stem;
            }
            break;
        }
    }

    return word;
}

auto EnglishConfig::step5a(std::string word) -> std::string
{
    // (m>1) E -> ""
    if (endsWith(word, "e"))
    {
        std::string stem = word.substr(0, word.length() - 1);
        int m = measure(stem);

        if (m > 1)
        {
            word = stem;
        }
        // (m=1 and not *o) E -> ""
        else if (m == 1)
        {
            // Check if ends with CVC pattern (not *o)
            if (stem.length() >= 3)
            {
                size_t len = stem.length();
                bool is_cvc = isConsonant(stem, len - 1) &&
                             !isConsonant(stem, len - 2) &&
                             isConsonant(stem, len - 3);

                if (is_cvc)
                {
                    char last = stem[len - 1];
                    if (last == 'w' || last == 'x' || last == 'y')
                    {
                        // Ends with *o pattern, don't remove E
                        return word;
                    }
                }

                word = stem;
            }
        }
    }

    return word;
}

auto EnglishConfig::step5b(std::string word) -> std::string
{
    // (m > 1 and *d and *L) -> single letter
    if (measure(word) > 1 && endsWithDoubleConsonant(word) && endsWith(word, "l"))
    {
        word = word.substr(0, word.length() - 1);
    }

    return word;
}

} // namespace scratchbird::core
