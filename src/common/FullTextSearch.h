/*
 *	PROGRAM:		Full-Text Search Types
 *	MODULE:			FullTextSearch.h
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

#ifndef SB_FULL_TEXT_SEARCH_H
#define SB_FULL_TEXT_SEARCH_H

#include "firebird/Interface.h"
#include "sb_exception.h"
#include "classes/fb_string.h"
#include <vector>
#include <map>
#include <set>

namespace ScratchBird {

// Text search configuration
enum TextSearchLanguage {
    TS_LANG_ENGLISH = 0,
    TS_LANG_SPANISH = 1,
    TS_LANG_FRENCH = 2,
    TS_LANG_GERMAN = 3,
    TS_LANG_SIMPLE = 99        // Simple configuration (no stemming)
};

// Lexeme weight (PostgreSQL-compatible)
enum LexemeWeight {
    WEIGHT_A = 0,              // Highest priority
    WEIGHT_B = 1,
    WEIGHT_C = 2,
    WEIGHT_D = 3               // Lowest priority (default)
};

// Position information for a lexeme
struct LexemePosition {
    USHORT position;           // Position in document (1-based)
    LexemeWeight weight;       // Weight of this occurrence
    
    LexemePosition(USHORT pos = 1, LexemeWeight w = WEIGHT_D) 
        : position(pos), weight(w) {}
        
    bool operator<(const LexemePosition& other) const {
        if (position != other.position) return position < other.position;
        return weight < other.weight;
    }
};

// Lexeme entry in TSVECTOR
struct Lexeme {
    string term;                           // Normalized lexeme
    std::vector<LexemePosition> positions; // Positions where lexeme appears
    
    Lexeme() {}
    Lexeme(const string& t) : term(t) {}
    
    void addPosition(USHORT pos, LexemeWeight weight = WEIGHT_D) {
        positions.push_back(LexemePosition(pos, weight));
    }
    
    bool hasWeight(LexemeWeight weight) const {
        for (const auto& pos : positions) {
            if (pos.weight == weight) return true;
        }
        return false;
    }
    
    ULONG getFrequency() const { return positions.size(); }
};

// TSVECTOR - Text search vector
class TSVector {
public:
    TSVector();
    TSVector(const char* text, TextSearchLanguage lang = TS_LANG_ENGLISH);
    TSVector(const string& text, TextSearchLanguage lang = TS_LANG_ENGLISH);
    
    // Construction from vector literal
    TSVector(const char* tsvector_literal);
    
    // Lexeme management
    void addLexeme(const string& term, USHORT position = 1, LexemeWeight weight = WEIGHT_D);
    void addLexeme(const Lexeme& lexeme);
    bool hasLexeme(const string& term) const;
    const Lexeme* getLexeme(const string& term) const;
    
    // Vector operations
    TSVector concatenate(const TSVector& other) const;              // ||
    TSVector operator||(const TSVector& other) const { return concatenate(other); }
    
    // Ranking and similarity
    double calculateRank(const class TSQuery& query) const;
    double calculateSimilarity(const TSVector& other) const;
    
    // Information
    ULONG getLexemeCount() const { return lexemes_.size(); }
    ULONG getTotalPositions() const;
    std::vector<string> getLexemeList() const;
    
    // String representation
    void toString(string& result) const;
    string toString() const;
    
    // Storage and indexing
    ULONG makeIndexKey(vary* buf) const;
    static ULONG getIndexKeyLength() { return 252; }
    
    // Parsing and validation
    void parseVector(const char* tsvector_literal);
    static bool isValidVector(const char* tsvector_literal);
    
private:
    std::map<string, Lexeme> lexemes_;     // Sorted lexemes for efficient access
    TextSearchLanguage language_;
    
    // Text processing
    void processText(const string& text);
    std::vector<string> tokenizeText(const string& text) const;
    string normalizeLexeme(const string& token) const;
    string stemWord(const string& word) const;
    bool isStopWord(const string& word) const;
    
    static void invalid_tsvector();
};

// Query node types for TSQUERY
enum TSQueryNodeType {
    TS_NODE_LEXEME = 0,        // Terminal lexeme
    TS_NODE_AND = 1,           // & operator
    TS_NODE_OR = 2,            // | operator  
    TS_NODE_NOT = 3,           // ! operator
    TS_NODE_PHRASE = 4         // <-> phrase operator
};

// TSQUERY node structure
struct TSQueryNode {
    TSQueryNodeType type;
    string lexeme;                         // For lexeme nodes
    LexemeWeight weight_filter;            // Weight filter for lexeme
    USHORT distance;                       // For phrase nodes
    std::shared_ptr<TSQueryNode> left;     // Left child
    std::shared_ptr<TSQueryNode> right;    // Right child
    
    TSQueryNode(TSQueryNodeType t = TS_NODE_LEXEME) 
        : type(t), weight_filter(WEIGHT_D), distance(1) {}
        
    bool isLeaf() const { return type == TS_NODE_LEXEME; }
};

// TSQUERY - Text search query
class TSQuery {
public:
    TSQuery();
    TSQuery(const char* query_string);
    TSQuery(const string& query_string);
    
    // Query construction
    static TSQuery lexeme(const string& term, LexemeWeight weight = WEIGHT_D);
    static TSQuery phrase(const std::vector<string>& terms, USHORT max_distance = 1);
    
    // Logical operators
    TSQuery operator&(const TSQuery& other) const;     // AND
    TSQuery operator|(const TSQuery& other) const;     // OR
    TSQuery operator!() const;                          // NOT
    
    // Phrase operator
    TSQuery followedBy(const TSQuery& other, USHORT distance = 1) const; // <->
    
    // Query evaluation
    bool matches(const TSVector& tsvector) const;
    double calculateCoverage(const TSVector& tsvector) const;
    std::vector<USHORT> findMatches(const TSVector& tsvector) const;
    
    // Query rewriting
    TSQuery rewrite(const std::map<string, string>& substitutions) const;
    TSQuery expandSynonyms(const std::map<string, std::vector<string>>& synonyms) const;
    
    // Information
    std::vector<string> extractLexemes() const;
    ULONG getComplexity() const;
    bool isEmpty() const { return !root_; }
    
    // String representation
    void toString(string& result) const;
    string toString() const;
    
    // Storage and indexing
    ULONG makeIndexKey(vary* buf) const;
    static ULONG getIndexKeyLength() { return 252; }
    
    // Parsing and validation
    void parseQuery(const char* query_string);
    static bool isValidQuery(const char* query_string);
    
private:
    std::shared_ptr<TSQueryNode> root_;
    TextSearchLanguage language_;
    
    // Query parsing
    std::shared_ptr<TSQueryNode> parseExpression(const std::vector<string>& tokens, ULONG& pos);
    std::shared_ptr<TSQueryNode> parseOr(const std::vector<string>& tokens, ULONG& pos);
    std::shared_ptr<TSQueryNode> parseAnd(const std::vector<string>& tokens, ULONG& pos);
    std::shared_ptr<TSQueryNode> parseNot(const std::vector<string>& tokens, ULONG& pos);
    std::shared_ptr<TSQueryNode> parseAtom(const std::vector<string>& tokens, ULONG& pos);
    
    // Query evaluation
    bool evaluateNode(const std::shared_ptr<TSQueryNode>& node, const TSVector& tsvector) const;
    bool matchesPhrase(const string& term1, const string& term2, USHORT distance, const TSVector& tsvector) const;
    
    // Utility functions
    std::vector<string> tokenizeQuery(const string& query) const;
    void nodeToString(const std::shared_ptr<TSQueryNode>& node, string& result) const;
    
    static void invalid_tsquery();
};

// Full-text search functions
class FullTextSearchUtils {
public:
    // Vector creation functions
    static TSVector toTSVector(const string& text, TextSearchLanguage lang = TS_LANG_ENGLISH);
    static TSVector toTSVector(const string& config, const string& text);
    
    // Query creation functions
    static TSQuery toTSQuery(const string& query, TextSearchLanguage lang = TS_LANG_ENGLISH);
    static TSQuery toTSQuery(const string& config, const string& query);
    static TSQuery plainToTSQuery(const string& text);  // Convert plain text to query
    static TSQuery phraseToTSQuery(const string& text); // Convert to phrase query
    
    // Ranking functions
    static double ts_rank(const TSVector& tsvector, const TSQuery& query, ULONG normalization = 0);
    static double ts_rank_cd(const TSVector& tsvector, const TSQuery& query, ULONG normalization = 0);
    
    // Headline generation
    static string ts_headline(const string& text, const TSQuery& query, 
                             const string& options = "");
    static string ts_headline(const string& config, const string& text, const TSQuery& query,
                             const string& options = "");
    
    // Query manipulation
    static TSQuery querytree(const TSQuery& query);  // Show query tree
    static TSQuery tsquery_phrase(const TSQuery& query1, const TSQuery& query2, USHORT distance = 1);
    
    // Statistics and information
    static ULONG numnode(const TSQuery& query);      // Number of nodes in query
    static std::vector<string> querytree_lexemes(const TSQuery& query);  // Extract all lexemes
    
    // Configuration management
    static std::vector<string> getAvailableConfigurations();
    static string getDefaultConfiguration();
    static void setDefaultConfiguration(const string& config);
};

// Type aliases
using TsVector = TSVector;
using TsQuery = TSQuery;

} // namespace ScratchBird

#endif // SB_FULL_TEXT_SEARCH_H