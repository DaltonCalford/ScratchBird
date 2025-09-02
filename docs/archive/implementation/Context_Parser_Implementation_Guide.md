# Context-Aware Parser Implementation Guide

## Overview

This guide provides a detailed implementation roadmap for ScratchBird's revolutionary context-aware parser.

## Core Implementation

### 1. Token Stream with Context

```cpp
// include/scratchbird/parser/context_token.h
#pragma once

#include <string>
#include <vector>
#include <memory>

namespace scratchbird::parser {

enum class TokenType {
    IDENTIFIER,
    NUMBER,
    STRING,
    OPERATOR,
    PUNCTUATION,
    WHITESPACE,
    COMMENT,
    EOF_TOKEN
};

enum class TokenRole {
    UNKNOWN,
    COMMAND,           // CREATE, SELECT, etc. at statement start
    OBJECT_TYPE,       // TABLE, VIEW, etc. after CREATE
    IDENTIFIER_NAME,   // Table/column/etc names
    DATA_TYPE,        // INTEGER, VARCHAR, etc.
    KEYWORD_CLAUSE,   // WHERE, FROM, etc. in context
    OPERATOR_BINARY,  // AND, OR, etc.
    OPERATOR_UNARY,   // NOT, etc.
    LITERAL_VALUE,    // String/number literals
    PUNCTUATION_STRUCTURAL  // (, ), ,, ;
};

struct Token {
    std::string text;
    std::string text_upper;  // Uppercase for comparison
    TokenType type;
    TokenRole role;          // Context-dependent role
    size_t line;
    size_t column;
    size_t position;         // Byte position in input
};

class TokenStream {
private:
    std::vector<Token> tokens;
    size_t current_pos = 0;
    
public:
    Token current() const {
        if (current_pos < tokens.size()) {
            return tokens[current_pos];
        }
        return Token{"", "", TokenType::EOF_TOKEN, TokenRole::UNKNOWN, 0, 0, 0};
    }
    
    Token peek(size_t ahead = 1) const {
        size_t pos = current_pos + ahead;
        if (pos < tokens.size()) {
            return tokens[pos];
        }
        return Token{"", "", TokenType::EOF_TOKEN, TokenRole::UNKNOWN, 0, 0, 0};
    }
    
    Token previous() const {
        if (current_pos > 0) {
            return tokens[current_pos - 1];
        }
        return Token{"", "", TokenType::EOF_TOKEN, TokenRole::UNKNOWN, 0, 0, 0};
    }
    
    void advance() {
        if (current_pos < tokens.size()) {
            current_pos++;
        }
    }
    
    bool at_end() const {
        return current_pos >= tokens.size() || 
               tokens[current_pos].type == TokenType::EOF_TOKEN;
    }
    
    void mark() {
        // Save position for backtracking
    }
    
    void reset_to_mark() {
        // Restore saved position
    }
};

} // namespace scratchbird::parser
```

### 2. Context State Machine

```cpp
// include/scratchbird/parser/context_state.h
#pragma once

#include <stack>
#include <map>
#include <functional>

namespace scratchbird::parser {

enum class ParseContext {
    // Top-level contexts
    STATEMENT_START,
    STATEMENT_END,
    
    // CREATE contexts
    CREATE_COMMAND,
    CREATE_OBJECT_TYPE,
    CREATE_TABLE_NAME,
    CREATE_TABLE_COLUMNS,
    CREATE_COLUMN_NAME,
    CREATE_COLUMN_TYPE,
    CREATE_COLUMN_CONSTRAINT,
    
    // SELECT contexts
    SELECT_COMMAND,
    SELECT_LIST,
    SELECT_EXPRESSION,
    FROM_CLAUSE,
    FROM_TABLE,
    WHERE_CLAUSE,
    WHERE_CONDITION,
    GROUP_BY_CLAUSE,
    ORDER_BY_CLAUSE,
    
    // Expression contexts
    EXPRESSION_START,
    EXPRESSION_OPERATOR,
    EXPRESSION_OPERAND,
    FUNCTION_CALL,
    FUNCTION_ARGS,
    
    // Many more...
};

class ContextStateMachine {
private:
    ParseContext current_context;
    std::stack<ParseContext> context_stack;
    std::map<ParseContext, std::function<ParseContext(const Token&)>> transitions;
    
public:
    ContextStateMachine() : current_context(ParseContext::STATEMENT_START) {
        initialize_transitions();
    }
    
    void initialize_transitions() {
        // Define state transitions
        transitions[ParseContext::STATEMENT_START] = [](const Token& tok) {
            if (tok.text_upper == "CREATE") return ParseContext::CREATE_OBJECT_TYPE;
            if (tok.text_upper == "SELECT") return ParseContext::SELECT_LIST;
            if (tok.text_upper == "INSERT") return ParseContext::INSERT_TARGET;
            // ... more commands
            return ParseContext::STATEMENT_START;  // Unknown, stay in start
        };
        
        transitions[ParseContext::CREATE_OBJECT_TYPE] = [](const Token& tok) {
            if (tok.text_upper == "TABLE") return ParseContext::CREATE_TABLE_NAME;
            if (tok.text_upper == "VIEW") return ParseContext::CREATE_VIEW_NAME;
            if (tok.text_upper == "INDEX") return ParseContext::CREATE_INDEX_NAME;
            // ... more object types
            return ParseContext::CREATE_OBJECT_TYPE;
        };
        
        transitions[ParseContext::CREATE_TABLE_NAME] = [](const Token& tok) {
            // Any identifier is valid as table name
            if (tok.type == TokenType::IDENTIFIER) {
                return ParseContext::CREATE_TABLE_COLUMNS;
            }
            return ParseContext::CREATE_TABLE_NAME;
        };
        
        // ... hundreds more transitions
    }
    
    ParseContext transition(const Token& token) {
        if (transitions.count(current_context)) {
            ParseContext next = transitions[current_context](token);
            
            // Handle context stack for nested structures
            if (is_push_context(next)) {
                context_stack.push(current_context);
            }
            if (is_pop_context(next) && !context_stack.empty()) {
                next = context_stack.top();
                context_stack.pop();
            }
            
            current_context = next;
        }
        return current_context;
    }
    
    bool is_complete() const {
        return current_context == ParseContext::STATEMENT_END ||
               is_terminal_context(current_context);
    }
    
    bool is_terminal_context(ParseContext ctx) const {
        // Contexts that represent complete statements
        static const std::set<ParseContext> terminals = {
            ParseContext::CREATE_TABLE_END,
            ParseContext::SELECT_COMPLETE,
            ParseContext::INSERT_COMPLETE,
            // ...
        };
        return terminals.count(ctx) > 0;
    }
};

} // namespace scratchbird::parser
```

### 3. Context-Aware Tokenizer

```cpp
// include/scratchbird/parser/context_tokenizer.h
#pragma once

namespace scratchbird::parser {

class ContextAwareTokenizer {
private:
    ContextStateMachine state_machine;
    std::set<std::string> minimal_reserved;  // Only ~10 words
    
public:
    ContextAwareTokenizer() {
        // Only structural punctuation is truly reserved
        minimal_reserved = {"(", ")", ",", ".", ";", "--", "/*", "*/"};
    }
    
    TokenRole classify_token(const Token& token, ParseContext context) {
        // Keywords are only keywords in specific contexts
        if (context == ParseContext::STATEMENT_START) {
            if (is_command_keyword(token.text_upper)) {
                return TokenRole::COMMAND;
            }
        }
        
        if (context == ParseContext::CREATE_OBJECT_TYPE) {
            if (is_object_type_keyword(token.text_upper)) {
                return TokenRole::OBJECT_TYPE;
            }
        }
        
        if (context == ParseContext::CREATE_COLUMN_TYPE) {
            if (is_data_type_keyword(token.text_upper)) {
                return TokenRole::DATA_TYPE;
            }
            // Could be custom type/domain
            return TokenRole::IDENTIFIER_NAME;
        }
        
        if (context == ParseContext::WHERE_CONDITION) {
            if (token.text_upper == "AND" || token.text_upper == "OR") {
                return TokenRole::OPERATOR_BINARY;
            }
            if (token.text_upper == "NOT") {
                return TokenRole::OPERATOR_UNARY;
            }
        }
        
        // In most contexts, anything can be an identifier
        if (token.type == TokenType::IDENTIFIER) {
            return TokenRole::IDENTIFIER_NAME;
        }
        
        return TokenRole::UNKNOWN;
    }
    
    bool is_command_keyword(const std::string& word) {
        static const std::set<std::string> commands = {
            "CREATE", "ALTER", "DROP", "SELECT", "INSERT", 
            "UPDATE", "DELETE", "WITH", "TRUNCATE", "GRANT", "REVOKE"
        };
        return commands.count(word) > 0;
    }
    
    bool is_truly_reserved(const std::string& word) {
        // Only structural elements are truly reserved
        return minimal_reserved.count(word) > 0;
    }
};

} // namespace scratchbird::parser
```

### 4. Statement Completion Detector

```cpp
// include/scratchbird/parser/completion_detector.h
#pragma once

namespace scratchbird::parser {

class CompletionDetector {
private:
    struct CompletionRule {
        ParseContext context;
        std::function<bool(const TokenStream&)> is_complete;
    };
    
    std::vector<CompletionRule> rules;
    
public:
    CompletionDetector() {
        initialize_rules();
    }
    
    void initialize_rules() {
        // CREATE TABLE is complete after closing paren
        rules.push_back({
            ParseContext::CREATE_TABLE_COLUMNS,
            [](const TokenStream& stream) {
                return stream.current().text == ")" && 
                       parentheses_balanced(stream);
            }
        });
        
        // SELECT is complete when no more clauses expected
        rules.push_back({
            ParseContext::SELECT_EXPRESSION,
            [](const TokenStream& stream) {
                Token next = stream.peek();
                // Check if next token starts a new clause
                static const std::set<std::string> clause_keywords = {
                    "FROM", "WHERE", "GROUP", "HAVING", "ORDER", 
                    "LIMIT", "OFFSET", "UNION", "INTERSECT", "EXCEPT"
                };
                
                if (clause_keywords.count(next.text_upper)) {
                    return false;  // More clauses coming
                }
                
                // Check if it's a new statement
                if (is_statement_start(next)) {
                    return true;  // Current statement complete
                }
                
                // At end of input
                if (next.type == TokenType::EOF_TOKEN) {
                    return true;
                }
                
                return false;
            }
        });
        
        // INSERT VALUES is complete after closing paren
        rules.push_back({
            ParseContext::INSERT_VALUES,
            [](const TokenStream& stream) {
                return stream.current().text == ")" &&
                       stream.peek().text_upper != ",";  // No more value sets
            }
        });
    }
    
    bool is_statement_complete(ParseContext context, const TokenStream& stream) {
        for (const auto& rule : rules) {
            if (rule.context == context) {
                return rule.is_complete(stream);
            }
        }
        
        // Default: check if context is terminal
        return is_terminal_context(context);
    }
    
    bool parentheses_balanced(const TokenStream& stream) {
        int depth = 0;
        for (size_t i = 0; i <= stream.current_position(); i++) {
            const Token& tok = stream.token_at(i);
            if (tok.text == "(") depth++;
            if (tok.text == ")") depth--;
        }
        return depth == 0;
    }
};

} // namespace scratchbird::parser
```

### 5. Intelligent Error Recovery

```cpp
// include/scratchbird/parser/error_recovery.h
#pragma once

#include <string>
#include <vector>

namespace scratchbird::parser {

class ErrorRecovery {
private:
    struct Suggestion {
        std::string text;
        double confidence;  // 0.0 to 1.0
    };
    
public:
    std::vector<Suggestion> suggest_corrections(
        ParseContext context, 
        const Token& unexpected,
        const TokenStream& stream
    ) {
        std::vector<Suggestion> suggestions;
        
        switch (context) {
            case ParseContext::STATEMENT_START:
                if (is_typo_of_command(unexpected.text)) {
                    suggestions.push_back({
                        "Did you mean: " + closest_command(unexpected.text),
                        0.9
                    });
                }
                break;
                
            case ParseContext::CREATE_OBJECT_TYPE:
                suggestions.push_back({
                    "Expected object type: TABLE, VIEW, INDEX, FUNCTION, etc.",
                    1.0
                });
                if (unexpected.type == TokenType::IDENTIFIER) {
                    suggestions.push_back({
                        "'" + unexpected.text + "' can be a table name. " +
                        "Did you forget 'TABLE'?",
                        0.7
                    });
                }
                break;
                
            case ParseContext::CREATE_COLUMN_TYPE:
                suggestions.push_back({
                    "Expected data type: INTEGER, VARCHAR, TIMESTAMP, etc.",
                    1.0
                });
                if (looks_like_column_name(unexpected.text)) {
                    suggestions.push_back({
                        "'" + unexpected.text + "' looks like a column name. " +
                        "Did you forget the data type for the previous column?",
                        0.8
                    });
                }
                break;
                
            case ParseContext::WHERE_CONDITION:
                if (unexpected.text_upper == "=") {
                    Token prev = stream.previous();
                    if (prev.text_upper == "WHERE") {
                        suggestions.push_back({
                            "Expected column name before '='",
                            1.0
                        });
                    }
                }
                break;
        }
        
        // Add context-aware help
        suggestions.push_back({
            "Current context: " + context_description(context),
            0.5
        });
        
        return suggestions;
    }
    
    std::string closest_command(const std::string& text) {
        // Use Levenshtein distance to find closest command
        static const std::vector<std::string> commands = {
            "CREATE", "SELECT", "INSERT", "UPDATE", "DELETE", 
            "ALTER", "DROP", "TRUNCATE"
        };
        
        std::string best_match;
        int min_distance = INT_MAX;
        
        for (const auto& cmd : commands) {
            int dist = levenshtein_distance(text, cmd);
            if (dist < min_distance) {
                min_distance = dist;
                best_match = cmd;
            }
        }
        
        return best_match;
    }
    
    bool is_typo_of_command(const std::string& text) {
        std::string upper = to_upper(text);
        
        // Common typos
        static const std::map<std::string, std::string> typos = {
            {"CRETE", "CREATE"},
            {"CREAT", "CREATE"},
            {"SELCT", "SELECT"},
            {"SLECT", "SELECT"},
            {"INSRT", "INSERT"},
            {"INSER", "INSERT"},
            {"UPDAT", "UPDATE"},
            {"DELET", "DELETE"},
            {"ALTE", "ALTER"},
            {"DRO", "DROP"}
        };
        
        return typos.count(upper) > 0;
    }
};

} // namespace scratchbird::parser
```

### 6. Interactive Mode Handler

```cpp
// include/scratchbird/parser/interactive_handler.h
#pragma once

namespace scratchbird::parser {

class InteractiveHandler {
private:
    ContextStateMachine state_machine;
    CompletionDetector completion_detector;
    ErrorRecovery error_recovery;
    std::string buffer;
    
public:
    struct ParseResult {
        bool complete;
        bool has_error;
        std::string error_message;
        std::vector<std::string> suggestions;
        std::string prompt_hint;  // What parser expects next
    };
    
    ParseResult process_line(const std::string& line) {
        buffer += line;
        
        TokenStream stream = tokenize(buffer);
        ParseResult result;
        
        // Process tokens through state machine
        while (!stream.at_end()) {
            Token tok = stream.current();
            ParseContext prev_context = state_machine.current_context();
            ParseContext new_context = state_machine.transition(tok);
            
            // Classify token based on context
            tok.role = classify_token(tok, prev_context);
            
            // Check for errors
            if (new_context == ParseContext::ERROR) {
                result.has_error = true;
                result.error_message = format_error(prev_context, tok);
                result.suggestions = error_recovery.suggest_corrections(
                    prev_context, tok, stream
                );
                return result;
            }
            
            stream.advance();
        }
        
        // Check if statement is complete
        result.complete = completion_detector.is_statement_complete(
            state_machine.current_context(), 
            stream
        );
        
        if (result.complete) {
            // Execute and reset
            buffer.clear();
            state_machine.reset();
            result.prompt_hint = "";
        } else {
            // Provide hint about what's expected
            result.prompt_hint = get_context_hint(state_machine.current_context());
        }
        
        return result;
    }
    
    std::string get_context_hint(ParseContext context) {
        switch (context) {
            case ParseContext::CREATE_TABLE_NAME:
                return "table name";
            case ParseContext::CREATE_COLUMN_NAME:
                return "column name";
            case ParseContext::CREATE_COLUMN_TYPE:
                return "data type";
            case ParseContext::WHERE_CONDITION:
                return "condition";
            case ParseContext::SELECT_LIST:
                return "columns or *";
            default:
                return "";
        }
    }
    
    std::vector<std::string> get_completions(const std::string& partial) {
        ParseContext ctx = state_machine.current_context();
        std::vector<std::string> completions;
        
        switch (ctx) {
            case ParseContext::STATEMENT_START:
                completions = {"CREATE", "SELECT", "INSERT", "UPDATE", 
                              "DELETE", "ALTER", "DROP"};
                break;
                
            case ParseContext::CREATE_OBJECT_TYPE:
                completions = {"TABLE", "VIEW", "INDEX", "FUNCTION", 
                              "PROCEDURE", "TRIGGER", "TYPE"};
                break;
                
            case ParseContext::CREATE_COLUMN_TYPE:
                completions = {"INTEGER", "BIGINT", "VARCHAR", "TEXT", 
                              "TIMESTAMP", "DATE", "BOOLEAN", "DECIMAL"};
                break;
                
            // ... more contexts
        }
        
        // Filter by partial match
        return filter_completions(completions, partial);
    }
};

} // namespace scratchbird::parser
```

## Testing Framework

```cpp
// tests/parser/context_parser_tests.cpp
#include <gtest/gtest.h>
#include "scratchbird/parser/context_parser.h"

using namespace scratchbird::parser;

TEST(ContextParser, KeywordsAsIdentifiers) {
    ContextParser parser;
    
    // All SQL keywords used as identifiers
    std::string sql = R"(
        CREATE TABLE select (
            from INTEGER,
            where VARCHAR(100),
            group INTEGER,
            order VARCHAR(50),
            by TIMESTAMP,
            having BOOLEAN,
            create DATE,
            table TEXT
        )
    )";
    
    auto result = parser.parse(sql);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.statements.size(), 1);
    
    auto& create_stmt = result.statements[0];
    ASSERT_EQ(create_stmt.table_name, "select");
    ASSERT_EQ(create_stmt.columns.size(), 8);
    ASSERT_EQ(create_stmt.columns[0].name, "from");
    ASSERT_EQ(create_stmt.columns[1].name, "where");
}

TEST(ContextParser, AutomaticCompletion) {
    ContextParser parser;
    
    // No semicolon needed
    std::string sql = "CREATE TABLE users (id INTEGER, name VARCHAR(100))";
    
    auto result = parser.parse(sql);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.complete);
    ASSERT_TRUE(result.ready_to_execute);
}

TEST(ContextParser, ContextualKeywords) {
    ContextParser parser;
    
    // 'timestamp' as table name, column name, and data type
    std::string sql = R"(
        CREATE TABLE timestamp (
            timestamp TIMESTAMP,
            integer INTEGER,
            varchar VARCHAR(100)
        )
    )";
    
    auto result = parser.parse(sql);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.statements[0].table_name, "timestamp");
    ASSERT_EQ(result.statements[0].columns[0].name, "timestamp");
    ASSERT_EQ(result.statements[0].columns[0].type, "TIMESTAMP");
}

TEST(ContextParser, IntelligentErrorRecovery) {
    ContextParser parser;
    
    // Typo in CREATE
    std::string sql = "CRETE TABLE users (id INTEGER)";
    
    auto result = parser.parse(sql);
    ASSERT_FALSE(result.success);
    ASSERT_TRUE(result.suggestions.size() > 0);
    ASSERT_TRUE(result.suggestions[0].find("CREATE") != std::string::npos);
}

TEST(ContextParser, MultiStatementParsing) {
    ContextParser parser;
    
    // Multiple statements without explicit semicolons
    std::string sql = R"(
        CREATE TABLE t1 (id INTEGER)
        CREATE TABLE t2 (id INTEGER)
        SELECT * FROM t1
    )";
    
    auto result = parser.parse_multiple(sql);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.statements.size(), 3);
}

TEST(ContextParser, ComplexNestedExpressions) {
    ContextParser parser;
    
    std::string sql = R"(
        SELECT 
            CASE 
                WHEN (SELECT COUNT(*) FROM orders WHERE status = 'pending') > 10 
                THEN 'high' 
                ELSE 'low' 
            END as load,
            (SELECT MAX(id) FROM users) as max_user
        FROM system_info
        WHERE date = CURRENT_DATE
    )";
    
    auto result = parser.parse(sql);
    ASSERT_TRUE(result.success);
    ASSERT_TRUE(result.complete);
}
```

## Performance Optimizations

### 1. JIT Compilation for Hot Paths

```cpp
class JITStateMachine {
    // Compile state transitions to native code for performance
    void compile_hot_transitions() {
        // Use LLVM or similar to JIT compile frequently used paths
    }
};
```

### 2. Token Caching

```cpp
class TokenCache {
    // Cache tokenization results for repeated queries
    std::unordered_map<std::string, TokenStream> cache;
};
```

### 3. Parallel Parsing for Multi-Statement

```cpp
class ParallelParser {
    // Parse independent statements in parallel
    std::vector<ParseResult> parse_parallel(const std::vector<std::string>& statements) {
        // Use thread pool for parallel processing
    }
};
```

## Integration Points

### 1. IDE Integration API

```cpp
class IDEIntegration {
    // Provide real-time parsing feedback
    ParseFeedback get_feedback(const std::string& sql, size_t cursor_pos);
    
    // Provide context-aware completions
    std::vector<Completion> get_completions(const std::string& sql, size_t cursor_pos);
    
    // Provide hover information
    HoverInfo get_hover_info(const std::string& sql, size_t cursor_pos);
};
```

### 2. Compatibility Layer

```cpp
class CompatibilityLayer {
    // Provide traditional parser interface for legacy tools
    TraditionalParseTree parse_traditional(const std::string& sql);
    
    // Convert context-aware AST to traditional AST
    TraditionalAST convert_ast(const ContextAwareAST& ast);
};
```

## Summary

The context-aware parser is ambitious but achievable through:

1. **Careful state machine design** - Track context precisely
2. **Multi-token lookahead** - Resolve ambiguities
3. **Intelligent error recovery** - Provide helpful feedback
4. **Performance optimization** - JIT compilation for hot paths
5. **Backward compatibility** - Support traditional parsing when needed

This parser will make ScratchBird the most user-friendly SQL database, eliminating reserved word conflicts and providing a natural SQL writing experience.