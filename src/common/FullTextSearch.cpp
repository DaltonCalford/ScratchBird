/*
 *	PROGRAM:		Full-Text Search Types
 *	MODULE:			FullTextSearch.cpp
 *	DESCRIPTION:	TSVECTOR and TSQUERY implementation
 *
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by ScratchBird Development Team
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2025 ScratchBird Development Team
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): _______________________________________.
 *
 */

#include "firebird.h"
#include "FullTextSearch.h"
#include <algorithm>
#include <sstream>
#include <cctype>
#include <cstring>
#include <regex>

namespace ScratchBird {

// Common English stop words
static const std::set<std::string> ENGLISH_STOP_WORDS = {
    "a", "an", "and", "are", "as", "at", "be", "by", "for", "from",
    "has", "he", "in", "is", "it", "its", "of", "on", "that", "the",
    "to", "was", "will", "with", "the", "this", "but", "they", "have",
    "had", "what", "said", "each", "which", "their", "time", "if"
};

// TSVector implementation

TSVector::TSVector() : language_(TS_LANG_ENGLISH) {
}

TSVector::TSVector(const char* text, TextSearchLanguage lang) 
    : language_(lang) {
    if (text) {
        processText(string(text));
    }
}

TSVector::TSVector(const string& text, TextSearchLanguage lang) 
    : language_(lang) {
    processText(text);
}

TSVector::TSVector(const char* tsvector_literal) 
    : language_(TS_LANG_ENGLISH) {
    parseVector(tsvector_literal);
}

void TSVector::addLexeme(const string& term, USHORT position, LexemeWeight weight) {
    string normalized = normalizeLexeme(term);
    if (normalized.empty()) return;
    
    auto& lexeme = lexemes_[normalized];
    lexeme.term = normalized;
    lexeme.addPosition(position, weight);
}

void TSVector::addLexeme(const Lexeme& lexeme) {
    if (!lexeme.term.empty()) {
        lexemes_[lexeme.term] = lexeme;
    }
}

bool TSVector::hasLexeme(const string& term) const {
    string normalized = normalizeLexeme(term);
    return lexemes_.find(normalized) != lexemes_.end();
}

const Lexeme* TSVector::getLexeme(const string& term) const {
    string normalized = normalizeLexeme(term);
    auto it = lexemes_.find(normalized);
    return (it != lexemes_.end()) ? &it->second : nullptr;
}

TSVector TSVector::concatenate(const TSVector& other) const {
    TSVector result = *this;
    
    // Add all lexemes from other vector
    for (const auto& pair : other.lexemes_) {
        const Lexeme& other_lexeme = pair.second;
        
        auto it = result.lexemes_.find(other_lexeme.term);
        if (it != result.lexemes_.end()) {
            // Merge positions
            for (const auto& pos : other_lexeme.positions) {
                it->second.addPosition(pos.position, pos.weight);
            }
        } else {
            // Add new lexeme
            result.lexemes_[other_lexeme.term] = other_lexeme;
        }
    }
    
    return result;
}

ULONG TSVector::getTotalPositions() const {
    ULONG total = 0;
    for (const auto& pair : lexemes_) {
        total += pair.second.getFrequency();
    }
    return total;
}

std::vector<string> TSVector::getLexemeList() const {
    std::vector<string> result;
    for (const auto& pair : lexemes_) {
        result.push_back(pair.first);
    }
    return result;
}

void TSVector::toString(string& result) const {
    std::ostringstream oss;
    
    bool first = true;
    for (const auto& pair : lexemes_) {
        if (!first) oss << " ";
        first = false;
        
        const Lexeme& lexeme = pair.second;
        oss << "'" << lexeme.term.c_str() << "'";
        
        // Add positions
        if (!lexeme.positions.empty()) {
            oss << ":";
            for (size_t i = 0; i < lexeme.positions.size(); ++i) {
                if (i > 0) oss << ",";
                oss << lexeme.positions[i].position;
                
                // Add weight if not default
                if (lexeme.positions[i].weight != WEIGHT_D) {
                    char weight_char = 'A' + static_cast<char>(lexeme.positions[i].weight);
                    oss << weight_char;
                }
            }
        }
    }
    
    result = oss.str().c_str();
}

string TSVector::toString() const {
    string result;
    toString(result);
    return result;
}

void TSVector::processText(const string& text) {
    std::vector<string> tokens = tokenizeText(text);
    
    USHORT position = 1;
    for (const auto& token : tokens) {
        string normalized = normalizeLexeme(token);
        if (!normalized.empty() && !isStopWord(normalized)) {
            addLexeme(normalized, position, WEIGHT_D);
        }
        position++;
    }
}

std::vector<string> TSVector::tokenizeText(const string& text) const {
    std::vector<string> tokens;
    std::regex word_regex(R"(\b\w+\b)");
    
    // Convert to std::string for regex operations
    std::string std_text(text.c_str());
    std::sregex_iterator iter(std_text.begin(), std_text.end(), word_regex);
    std::sregex_iterator end;
    
    while (iter != end) {
        tokens.push_back(string(iter->str().c_str()));
        ++iter;
    }
    
    return tokens;
}

string TSVector::normalizeLexeme(const string& token) const {
    if (token.empty()) return "";
    
    // Convert to lowercase
    string normalized = token;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    // Apply stemming if language supports it
    if (language_ != TS_LANG_SIMPLE) {
        normalized = stemWord(normalized);
    }
    
    return normalized;
}

string TSVector::stemWord(const string& word) const {
    // Simplified English stemming (Porter stemmer would be more complete)
    string result = word;
    
    // Remove common suffixes
    if (result.length() > 4) {
        if (result.substr(result.length() - 3) == "ing") {
            result = result.substr(0, result.length() - 3);
        } else if (result.substr(result.length() - 2) == "ed") {
            result = result.substr(0, result.length() - 2);
        } else if (result.substr(result.length() - 1) == "s") {
            result = result.substr(0, result.length() - 1);
        }
    }
    
    return result;
}

bool TSVector::isStopWord(const string& word) const {
    if (language_ == TS_LANG_SIMPLE) return false;
    
    return ENGLISH_STOP_WORDS.find(std::string(word.c_str())) != ENGLISH_STOP_WORDS.end();
}

ULONG TSVector::makeIndexKey(vary* buf) const {
    string str_repr = toString();
    ULONG copy_length = std::min(static_cast<ULONG>(str_repr.length()), 
                                static_cast<ULONG>(252 - sizeof(USHORT)));
    
    buf->vary_length = static_cast<USHORT>(copy_length);
    if (copy_length > 0) {
        memcpy(buf->vary_string, str_repr.c_str(), copy_length);
    }
    
    return sizeof(USHORT) + copy_length;
}

void TSVector::parseVector(const char* tsvector_literal) {
    if (!tsvector_literal) {
        invalid_tsvector();
        return;
    }
    
    // Parse format: 'word1':1,2A 'word2':3,4B,5
    std::string std_input(tsvector_literal);
    std::regex lexeme_regex(R"('([^']+)':([0-9,A-D]+))");
    std::sregex_iterator iter(std_input.begin(), std_input.end(), lexeme_regex);
    std::sregex_iterator end;
    
    while (iter != end) {
        string term(iter->str(1).c_str());
        string positions_str(iter->str(2).c_str());
        
        Lexeme lexeme(term);
        
        // Parse positions: 1,2A,3B
        std::istringstream pos_stream(positions_str.c_str());
        std::string pos_token;
        
        while (std::getline(pos_stream, pos_token, ',')) {
            if (pos_token.empty()) continue;
            
            // Extract position number and weight
            USHORT position = 0;
            LexemeWeight weight = WEIGHT_D;
            
            size_t i = 0;
            while (i < pos_token.length() && std::isdigit(pos_token[i])) {
                position = position * 10 + (pos_token[i] - '0');
                i++;
            }
            
            if (i < pos_token.length()) {
                char weight_char = pos_token[i];
                if (weight_char >= 'A' && weight_char <= 'D') {
                    weight = static_cast<LexemeWeight>(weight_char - 'A');
                }
            }
            
            lexeme.addPosition(position, weight);
        }
        
        addLexeme(lexeme);
        ++iter;
    }
}

void TSVector::invalid_tsvector() {
    throw std::invalid_argument("Invalid TSVECTOR format");
}

// TSQuery implementation

TSQuery::TSQuery() : language_(TS_LANG_ENGLISH) {
}

TSQuery::TSQuery(const char* query_string) : language_(TS_LANG_ENGLISH) {
    parseQuery(query_string);
}

TSQuery::TSQuery(const string& query_string) : language_(TS_LANG_ENGLISH) {
    parseQuery(query_string.c_str());
}

TSQuery TSQuery::lexeme(const string& term, LexemeWeight weight) {
    TSQuery query;
    query.root_ = std::make_shared<TSQueryNode>(TS_NODE_LEXEME);
    query.root_->lexeme = term;
    query.root_->weight_filter = weight;
    return query;
}

TSQuery TSQuery::operator&(const TSQuery& other) const {
    TSQuery result;
    result.root_ = std::make_shared<TSQueryNode>(TS_NODE_AND);
    result.root_->left = root_;
    result.root_->right = other.root_;
    return result;
}

TSQuery TSQuery::operator|(const TSQuery& other) const {
    TSQuery result;
    result.root_ = std::make_shared<TSQueryNode>(TS_NODE_OR);
    result.root_->left = root_;
    result.root_->right = other.root_;
    return result;
}

TSQuery TSQuery::operator!() const {
    TSQuery result;
    result.root_ = std::make_shared<TSQueryNode>(TS_NODE_NOT);
    result.root_->left = root_;
    return result;
}

bool TSQuery::matches(const TSVector& tsvector) const {
    if (!root_) return false;
    return evaluateNode(root_, tsvector);
}

std::vector<string> TSQuery::extractLexemes() const {
    std::vector<string> lexemes;
    // This would need a recursive tree traversal to extract all lexemes
    return lexemes;
}

void TSQuery::toString(string& result) const {
    if (!root_) {
        result = "";
        return;
    }
    
    nodeToString(root_, result);
}

string TSQuery::toString() const {
    string result;
    toString(result);
    return result;
}

ULONG TSQuery::makeIndexKey(vary* buf) const {
    string str_repr = toString();
    ULONG copy_length = std::min(static_cast<ULONG>(str_repr.length()), 
                                static_cast<ULONG>(252 - sizeof(USHORT)));
    
    buf->vary_length = static_cast<USHORT>(copy_length);
    if (copy_length > 0) {
        memcpy(buf->vary_string, str_repr.c_str(), copy_length);
    }
    
    return sizeof(USHORT) + copy_length;
}

void TSQuery::parseQuery(const char* query_string) {
    if (!query_string) {
        invalid_tsquery();
        return;
    }
    
    // Simplified query parsing - would need a proper expression parser
    string query(query_string);
    
    // For now, just create a simple lexeme query
    root_ = std::make_shared<TSQueryNode>(TS_NODE_LEXEME);
    root_->lexeme = query;
}

bool TSQuery::evaluateNode(const std::shared_ptr<TSQueryNode>& node, const TSVector& tsvector) const {
    if (!node) return false;
    
    switch (node->type) {
        case TS_NODE_LEXEME:
            return tsvector.hasLexeme(node->lexeme);
            
        case TS_NODE_AND:
            return evaluateNode(node->left, tsvector) && evaluateNode(node->right, tsvector);
            
        case TS_NODE_OR:
            return evaluateNode(node->left, tsvector) || evaluateNode(node->right, tsvector);
            
        case TS_NODE_NOT:
            return !evaluateNode(node->left, tsvector);
            
        case TS_NODE_PHRASE:
            // Phrase matching would be more complex
            return evaluateNode(node->left, tsvector) && evaluateNode(node->right, tsvector);
            
        default:
            return false;
    }
}

void TSQuery::nodeToString(const std::shared_ptr<TSQueryNode>& node, string& result) const {
    if (!node) {
        result = "";
        return;
    }
    
    switch (node->type) {
        case TS_NODE_LEXEME:
            result = "'" + node->lexeme + "'";
            break;
            
        case TS_NODE_AND: {
            string left, right;
            nodeToString(node->left, left);
            nodeToString(node->right, right);
            result = "(" + left + " & " + right + ")";
            break;
        }
        
        case TS_NODE_OR: {
            string left, right;
            nodeToString(node->left, left);
            nodeToString(node->right, right);
            result = "(" + left + " | " + right + ")";
            break;
        }
        
        case TS_NODE_NOT: {
            string left;
            nodeToString(node->left, left);
            result = "!" + left;
            break;
        }
        
        default:
            result = "";
            break;
    }
}

void TSQuery::invalid_tsquery() {
    throw std::invalid_argument("Invalid TSQUERY format");
}

// FullTextSearchUtils implementation

TSVector FullTextSearchUtils::toTSVector(const string& text, TextSearchLanguage lang) {
    return TSVector(text, lang);
}

TSQuery FullTextSearchUtils::toTSQuery(const string& query, TextSearchLanguage lang) {
    TSQuery result;
    result.parseQuery(query.c_str());
    return result;
}

double FullTextSearchUtils::ts_rank(const TSVector& tsvector, const TSQuery& query, ULONG normalization) {
    // Simplified ranking algorithm
    if (!query.matches(tsvector)) {
        return 0.0;
    }
    
    // Basic frequency-based ranking
    ULONG lexeme_count = tsvector.getLexemeCount();
    ULONG total_positions = tsvector.getTotalPositions();
    
    if (total_positions == 0) return 0.0;
    
    double rank = static_cast<double>(lexeme_count) / static_cast<double>(total_positions);
    
    // Apply normalization
    if (normalization & 1) {
        rank = rank / (rank + 1.0);  // Document length normalization
    }
    
    return rank;
}

string FullTextSearchUtils::ts_headline(const string& text, const TSQuery& query, const string& options) {
    // Simplified headline generation
    // In a real implementation, this would highlight matching terms
    if (text.length() <= 100) {
        return text;
    }
    
    // Extract first 100 characters as headline
    return text.substr(0, 97) + "...";
}

} // namespace ScratchBird