/**
 * MySQL Parser Lexer Implementation
 *
 * Implements MySQL 8.0 SQL tokenization with all reserved keywords,
 * operators, literals, and MySQL-specific constructs.
 */

#include "scratchbird/parser/mysql/mysql_lexer.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstring>

namespace scratchbird::parser::mysql {

// ============================================================================
// StringPool Implementation
// ============================================================================

uint32_t StringPool::intern(std::string_view str) {
    auto it = index_.find(str);
    if (it != index_.end()) {
        return it->second;
    }

    uint32_t id = static_cast<uint32_t>(strings_.size());
    strings_.emplace_back(str);
    index_[strings_.back()] = id;
    return id;
}

std::string_view StringPool::get(uint32_t id) const {
    if (id >= strings_.size()) {
        return "";
    }
    return strings_[id];
}

void StringPool::clear() {
    strings_.clear();
    index_.clear();
}

// ============================================================================
// Keyword Table
// ============================================================================

std::unordered_map<std::string_view, TokenType> Lexer::initKeywords() {
    return {
        // Statement initiators
        {"SELECT", TokenType::KW_SELECT},
        {"INSERT", TokenType::KW_INSERT},
        {"UPDATE", TokenType::KW_UPDATE},
        {"DELETE", TokenType::KW_DELETE},
        {"REPLACE", TokenType::KW_REPLACE},
        {"CREATE", TokenType::KW_CREATE},
        {"ALTER", TokenType::KW_ALTER},
        {"DROP", TokenType::KW_DROP},
        {"TRUNCATE", TokenType::KW_TRUNCATE},
        {"GRANT", TokenType::KW_GRANT},
        {"REVOKE", TokenType::KW_REVOKE},
        {"CALL", TokenType::KW_CALL},
        {"USE", TokenType::KW_USE},
        {"SHOW", TokenType::KW_SHOW},
        {"DESCRIBE", TokenType::KW_DESCRIBE},
        {"EXPLAIN", TokenType::KW_EXPLAIN},
        {"SET", TokenType::KW_SET},
        {"BEGIN", TokenType::KW_BEGIN},
        {"START", TokenType::KW_START},
        {"COMMIT", TokenType::KW_COMMIT},
        {"ROLLBACK", TokenType::KW_ROLLBACK},
        {"SAVEPOINT", TokenType::KW_SAVEPOINT},
        {"RELEASE", TokenType::KW_RELEASE},
        {"LOCK", TokenType::KW_LOCK},
        {"UNLOCK", TokenType::KW_UNLOCK},

        // Clause keywords
        {"FROM", TokenType::KW_FROM},
        {"WHERE", TokenType::KW_WHERE},
        {"GROUP", TokenType::KW_GROUP},
        {"HAVING", TokenType::KW_HAVING},
        {"ORDER", TokenType::KW_ORDER},
        {"BY", TokenType::KW_BY},
        {"LIMIT", TokenType::KW_LIMIT},
        {"OFFSET", TokenType::KW_OFFSET},
        {"UNION", TokenType::KW_UNION},
        {"INTERSECT", TokenType::KW_INTERSECT},
        {"EXCEPT", TokenType::KW_EXCEPT},
        {"WITH", TokenType::KW_WITH},
        {"RECURSIVE", TokenType::KW_RECURSIVE},

        // Join keywords
        {"JOIN", TokenType::KW_JOIN},
        {"INNER", TokenType::KW_INNER},
        {"LEFT", TokenType::KW_LEFT},
        {"RIGHT", TokenType::KW_RIGHT},
        {"CROSS", TokenType::KW_CROSS},
        {"OUTER", TokenType::KW_OUTER},
        {"NATURAL", TokenType::KW_NATURAL},
        {"ON", TokenType::KW_ON},
        {"USING", TokenType::KW_USING},
        {"STRAIGHT_JOIN", TokenType::KW_STRAIGHT_JOIN},
        {"LATERAL", TokenType::KW_LATERAL},

        // Expression keywords
        {"AND", TokenType::KW_AND},
        {"OR", TokenType::KW_OR},
        {"XOR", TokenType::KW_XOR},
        {"NOT", TokenType::KW_NOT},
        {"IS", TokenType::KW_IS},
        {"IN", TokenType::KW_IN},
        {"BETWEEN", TokenType::KW_BETWEEN},
        {"LIKE", TokenType::KW_LIKE},
        {"REGEXP", TokenType::KW_REGEXP},
        {"RLIKE", TokenType::KW_RLIKE},
        {"CASE", TokenType::KW_CASE},
        {"WHEN", TokenType::KW_WHEN},
        {"THEN", TokenType::KW_THEN},
        {"ELSE", TokenType::KW_ELSE},
        {"END", TokenType::KW_END},
        {"NULL", TokenType::KW_NULL},
        {"TRUE", TokenType::KW_TRUE},
        {"FALSE", TokenType::KW_FALSE},
        {"EXISTS", TokenType::KW_EXISTS},
        {"CAST", TokenType::KW_CAST},
        {"CONVERT", TokenType::KW_CONVERT},
        {"AS", TokenType::KW_AS},
        {"ESCAPE", TokenType::KW_ESCAPE},

        // DML keywords
        {"INTO", TokenType::KW_INTO},
        {"VALUES", TokenType::KW_VALUES},
        {"VALUE", TokenType::KW_VALUE},
        {"DEFAULT", TokenType::KW_DEFAULT},
        {"DUPLICATE", TokenType::KW_DUPLICATE},
        {"KEY", TokenType::KW_KEY},
        {"IGNORE", TokenType::KW_IGNORE},
        {"LOW_PRIORITY", TokenType::KW_LOW_PRIORITY},
        {"DELAYED", TokenType::KW_DELAYED},
        {"HIGH_PRIORITY", TokenType::KW_HIGH_PRIORITY},
        {"QUICK", TokenType::KW_QUICK},
        {"RETURNING", TokenType::KW_RETURNING},

        // DDL keywords
        {"TABLE", TokenType::KW_TABLE},
        {"TABLES", TokenType::KW_TABLES},
        {"DATABASE", TokenType::KW_DATABASE},
        {"DATABASES", TokenType::KW_DATABASES},
        {"SCHEMA", TokenType::KW_SCHEMA},
        {"SCHEMAS", TokenType::KW_SCHEMAS},
        {"INDEX", TokenType::KW_INDEX},
        {"INDEXES", TokenType::KW_INDEXES},
        {"VIEW", TokenType::KW_VIEW},
        {"PROCEDURE", TokenType::KW_PROCEDURE},
        {"FUNCTION", TokenType::KW_FUNCTION},
        {"TRIGGER", TokenType::KW_TRIGGER},
        {"EVENT", TokenType::KW_EVENT},
        {"COLUMN", TokenType::KW_COLUMN},
        {"COLUMNS", TokenType::KW_COLUMNS},
        {"ADD", TokenType::KW_ADD},
        {"CHANGE", TokenType::KW_CHANGE},
        {"MODIFY", TokenType::KW_MODIFY},
        {"RENAME", TokenType::KW_RENAME},
        {"TO", TokenType::KW_TO},
        {"IF", TokenType::KW_IF},
        {"TEMPORARY", TokenType::KW_TEMPORARY},
        {"UNIQUE", TokenType::KW_UNIQUE},
        {"PRIMARY", TokenType::KW_PRIMARY},
        {"FOREIGN", TokenType::KW_FOREIGN},
        {"REFERENCES", TokenType::KW_REFERENCES},
        {"CONSTRAINT", TokenType::KW_CONSTRAINT},
        {"CHECK", TokenType::KW_CHECK},
        {"CASCADE", TokenType::KW_CASCADE},
        {"RESTRICT", TokenType::KW_RESTRICT},
        {"NO", TokenType::KW_NO},
        {"ACTION", TokenType::KW_ACTION},
        {"FULLTEXT", TokenType::KW_FULLTEXT},
        {"SPATIAL", TokenType::KW_SPATIAL},
        {"HASH", TokenType::KW_HASH},
        {"BTREE", TokenType::KW_BTREE},
        {"ENGINE", TokenType::KW_ENGINE},
        {"CHARSET", TokenType::KW_CHARSET},
        {"CHARACTER", TokenType::KW_CHARACTER},
        {"COLLATE", TokenType::KW_COLLATE},
        {"AUTO_INCREMENT", TokenType::KW_AUTO_INCREMENT},
        {"COMMENT", TokenType::KW_COMMENT},
        {"PARTITION", TokenType::KW_PARTITION},
        {"PARTITIONS", TokenType::KW_PARTITIONS},
        {"ALGORITHM", TokenType::KW_ALGORITHM},
        {"DEFINER", TokenType::KW_DEFINER},
        {"INVOKER", TokenType::KW_INVOKER},
        {"SQL", TokenType::KW_SQL},
        {"SECURITY", TokenType::KW_SECURITY},

        // Type keywords
        {"TINYINT", TokenType::KW_TINYINT},
        {"SMALLINT", TokenType::KW_SMALLINT},
        {"MEDIUMINT", TokenType::KW_MEDIUMINT},
        {"INT", TokenType::KW_INT},
        {"INTEGER", TokenType::KW_INTEGER},
        {"BIGINT", TokenType::KW_BIGINT},
        {"FLOAT", TokenType::KW_FLOAT},
        {"DOUBLE", TokenType::KW_DOUBLE},
        {"REAL", TokenType::KW_REAL},
        {"DECIMAL", TokenType::KW_DECIMAL},
        {"NUMERIC", TokenType::KW_NUMERIC},
        {"BIT", TokenType::KW_BIT},
        {"BOOL", TokenType::KW_BOOL},
        {"BOOLEAN", TokenType::KW_BOOLEAN},
        {"CHAR", TokenType::KW_CHAR},
        {"VARCHAR", TokenType::KW_VARCHAR},
        {"BINARY", TokenType::KW_BINARY},
        {"VARBINARY", TokenType::KW_VARBINARY},
        {"TINYTEXT", TokenType::KW_TINYTEXT},
        {"TEXT", TokenType::KW_TEXT},
        {"MEDIUMTEXT", TokenType::KW_MEDIUMTEXT},
        {"LONGTEXT", TokenType::KW_LONGTEXT},
        {"TINYBLOB", TokenType::KW_TINYBLOB},
        {"BLOB", TokenType::KW_BLOB},
        {"MEDIUMBLOB", TokenType::KW_MEDIUMBLOB},
        {"LONGBLOB", TokenType::KW_LONGBLOB},
        {"DATE", TokenType::KW_DATE},
        {"TIME", TokenType::KW_TIME},
        {"DATETIME", TokenType::KW_DATETIME},
        {"TIMESTAMP", TokenType::KW_TIMESTAMP},
        {"YEAR", TokenType::KW_YEAR},
        {"ENUM", TokenType::KW_ENUM},
        {"JSON", TokenType::KW_JSON},
        {"GEOMETRY", TokenType::KW_GEOMETRY},
        {"POINT", TokenType::KW_POINT},
        {"LINESTRING", TokenType::KW_LINESTRING},
        {"POLYGON", TokenType::KW_POLYGON},

        // Type modifiers
        {"UNSIGNED", TokenType::KW_UNSIGNED},
        {"ZEROFILL", TokenType::KW_ZEROFILL},
        {"PRECISION", TokenType::KW_PRECISION},
        {"VARYING", TokenType::KW_VARYING},
        {"ZONE", TokenType::KW_ZONE},

        // Aggregate keywords
        {"ALL", TokenType::KW_ALL},
        {"DISTINCT", TokenType::KW_DISTINCT},
        {"DISTINCTROW", TokenType::KW_DISTINCTROW},
        {"ASC", TokenType::KW_ASC},
        {"DESC", TokenType::KW_DESC},
        {"FIRST", TokenType::KW_FIRST},
        {"LAST", TokenType::KW_LAST},
        {"NULLS", TokenType::KW_NULLS},
        {"ROLLUP", TokenType::KW_ROLLUP},
        {"CUBE", TokenType::KW_CUBE},
        {"GROUPING", TokenType::KW_GROUPING},

        // Window function keywords
        {"OVER", TokenType::KW_OVER},
        {"WINDOW", TokenType::KW_WINDOW},
        {"ROWS", TokenType::KW_ROWS},
        {"RANGE", TokenType::KW_RANGE},
        {"GROUPS", TokenType::KW_GROUPS},
        {"UNBOUNDED", TokenType::KW_UNBOUNDED},
        {"PRECEDING", TokenType::KW_PRECEDING},
        {"FOLLOWING", TokenType::KW_FOLLOWING},
        {"CURRENT", TokenType::KW_CURRENT},
        {"ROW", TokenType::KW_ROW},

        // Transaction keywords
        {"TRANSACTION", TokenType::KW_TRANSACTION},
        {"WORK", TokenType::KW_WORK},
        {"CHAIN", TokenType::KW_CHAIN},
        {"READ", TokenType::KW_READ},
        {"WRITE", TokenType::KW_WRITE},
        {"ONLY", TokenType::KW_ONLY},
        {"COMMITTED", TokenType::KW_COMMITTED},
        {"UNCOMMITTED", TokenType::KW_UNCOMMITTED},
        {"REPEATABLE", TokenType::KW_REPEATABLE},
        {"SERIALIZABLE", TokenType::KW_SERIALIZABLE},
        {"ISOLATION", TokenType::KW_ISOLATION},
        {"LEVEL", TokenType::KW_LEVEL},

        // Stored program keywords
        {"DECLARE", TokenType::KW_DECLARE},
        {"HANDLER", TokenType::KW_HANDLER},
        {"CONTINUE", TokenType::KW_CONTINUE},
        {"EXIT", TokenType::KW_EXIT},
        {"UNDO", TokenType::KW_UNDO},
        {"SQLSTATE", TokenType::KW_SQLSTATE},
        {"SQLEXCEPTION", TokenType::KW_SQLEXCEPTION},
        {"SQLWARNING", TokenType::KW_SQLWARNING},
        {"FOUND", TokenType::KW_FOUND},
        {"FOR", TokenType::KW_FOR},
        {"EACH", TokenType::KW_EACH},
        {"LOOP", TokenType::KW_LOOP},
        {"WHILE", TokenType::KW_WHILE},
        {"REPEAT", TokenType::KW_REPEAT},
        {"UNTIL", TokenType::KW_UNTIL},
        {"LEAVE", TokenType::KW_LEAVE},
        {"ITERATE", TokenType::KW_ITERATE},
        {"RETURN", TokenType::KW_RETURN},
        {"RETURNS", TokenType::KW_RETURNS},
        {"DETERMINISTIC", TokenType::KW_DETERMINISTIC},
        {"MODIFIES", TokenType::KW_MODIFIES},
        {"READS", TokenType::KW_READS},
        {"CONTAINS", TokenType::KW_CONTAINS},
        {"LANGUAGE", TokenType::KW_LANGUAGE},
        {"INOUT", TokenType::KW_INOUT},
        {"OUT", TokenType::KW_OUT},
        {"CURSOR", TokenType::KW_CURSOR},
        {"OPEN", TokenType::KW_OPEN},
        {"CLOSE", TokenType::KW_CLOSE},
        {"FETCH", TokenType::KW_FETCH},
        {"SIGNAL", TokenType::KW_SIGNAL},
        {"RESIGNAL", TokenType::KW_RESIGNAL},
        {"CONDITION", TokenType::KW_CONDITION},

        // Trigger keywords
        {"BEFORE", TokenType::KW_BEFORE},
        {"AFTER", TokenType::KW_AFTER},
        {"FOLLOWS", TokenType::KW_FOLLOWS},
        {"PRECEDES", TokenType::KW_PRECEDES},

        // Security keywords
        {"USER", TokenType::KW_USER},
        {"ROLE", TokenType::KW_ROLE},
        {"IDENTIFIED", TokenType::KW_IDENTIFIED},
        {"PASSWORD", TokenType::KW_PASSWORD},
        {"OPTION", TokenType::KW_OPTION},
        {"ADMIN", TokenType::KW_ADMIN},
        {"PUBLIC", TokenType::KW_PUBLIC},
        {"PRIVILEGES", TokenType::KW_PRIVILEGES},
        {"USAGE", TokenType::KW_USAGE},

        // Misc keywords
        {"FORCE", TokenType::KW_FORCE},
        {"MATCH", TokenType::KW_MATCH},
        {"FULL", TokenType::KW_FULL},
        {"PARTIAL", TokenType::KW_PARTIAL},
        {"SIMPLE", TokenType::KW_SIMPLE},
        {"GENERATED", TokenType::KW_GENERATED},
        {"ALWAYS", TokenType::KW_ALWAYS},
        {"STORED", TokenType::KW_STORED},
        {"VIRTUAL", TokenType::KW_VIRTUAL},
        {"VISIBLE", TokenType::KW_VISIBLE},
        {"INVISIBLE", TokenType::KW_INVISIBLE},
        {"ANALYZE", TokenType::KW_ANALYZE},
        {"OPTIMIZE", TokenType::KW_OPTIMIZE},
        {"DATA", TokenType::KW_DATA},
        {"INFILE", TokenType::KW_INFILE},
        {"OUTFILE", TokenType::KW_OUTFILE},
        {"DUMPFILE", TokenType::KW_DUMPFILE},
        {"LOAD", TokenType::KW_LOAD},
        {"LINES", TokenType::KW_LINES},
        {"TERMINATED", TokenType::KW_TERMINATED},
        {"ENCLOSED", TokenType::KW_ENCLOSED},
        {"ESCAPED", TokenType::KW_ESCAPED},
        {"OPTIONALLY", TokenType::KW_OPTIONALLY},
        {"STARTING", TokenType::KW_STARTING},
        {"LOCAL", TokenType::KW_LOCAL},
        {"GLOBAL", TokenType::KW_GLOBAL},
        {"SESSION", TokenType::KW_SESSION},
        {"STATUS", TokenType::KW_STATUS},
        {"VARIABLES", TokenType::KW_VARIABLES},
        {"PROCESSLIST", TokenType::KW_PROCESSLIST},
        {"WARNINGS", TokenType::KW_WARNINGS},
        {"ERRORS", TokenType::KW_ERRORS},
        {"PROFILE", TokenType::KW_PROFILE},
        {"PROFILES", TokenType::KW_PROFILES},

        // DIV and MOD as operators (keywords)
        {"DIV", TokenType::DIV},
        {"MOD", TokenType::PERCENT},
    };
}

const std::unordered_map<std::string_view, TokenType> Lexer::keywords_ = Lexer::initKeywords();

// ============================================================================
// Lexer Implementation
// ============================================================================

Lexer::Lexer(std::string_view input)
    : input_(input)
    , current_(0)
    , location_()
    , has_peeked_(false)
{
}

Token Lexer::nextToken() {
    if (has_peeked_) {
        has_peeked_ = false;
        return peeked_token_;
    }
    return scanToken();
}

Token Lexer::peek() {
    if (!has_peeked_) {
        peeked_token_ = scanToken();
        has_peeked_ = true;
    }
    return peeked_token_;
}

char Lexer::advance() {
    if (current_ >= input_.size()) {
        return '\0';
    }
    char c = input_[current_++];
    if (c == '\n') {
        location_.line++;
        location_.column = 1;
    } else {
        location_.column++;
    }
    location_.offset = static_cast<uint32_t>(current_);
    return c;
}

char Lexer::peek_char() const {
    if (current_ >= input_.size()) {
        return '\0';
    }
    return input_[current_];
}

char Lexer::peek_char_ahead(size_t offset) const {
    if (current_ + offset >= input_.size()) {
        return '\0';
    }
    return input_[current_ + offset];
}

bool Lexer::match(char expected) {
    if (isAtEnd() || input_[current_] != expected) {
        return false;
    }
    advance();
    return true;
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek_char();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                advance();
                break;
            case '-':
                if (peek_char_ahead(1) == '-') {
                    skipLineComment();
                } else {
                    return;
                }
                break;
            case '#':
                skipLineComment();
                break;
            case '/':
                if (peek_char_ahead(1) == '*') {
                    skipBlockComment();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

void Lexer::skipLineComment() {
    // Skip until end of line
    while (!isAtEnd() && peek_char() != '\n') {
        advance();
    }
}

void Lexer::skipBlockComment() {
    // Skip /* ... */
    advance(); // '/'
    advance(); // '*'

    while (!isAtEnd()) {
        if (peek_char() == '*' && peek_char_ahead(1) == '/') {
            advance(); // '*'
            advance(); // '/'
            return;
        }
        advance();
    }
    // Unterminated block comment - just consume rest
}

Token Lexer::scanToken() {
    skipWhitespace();

    if (isAtEnd()) {
        return Token::makeEOF(location_);
    }

    SourceLocation start_loc = location_;
    char c = peek_char();

    // Hex literals starting with 0x (must check BEFORE number)
    if (c == '0' && (peek_char_ahead(1) == 'x' || peek_char_ahead(1) == 'X')) {
        return scanHexLiteral();
    }

    // Hex literals with X prefix: X'...' (must check BEFORE identifier)
    if ((c == 'X' || c == 'x') && peek_char_ahead(1) == '\'') {
        return scanHexLiteral();
    }

    // Bit literals: b'...' or B'...' (must check BEFORE identifier)
    if ((c == 'b' || c == 'B') && peek_char_ahead(1) == '\'') {
        return scanBitLiteral();
    }

    // Identifiers and keywords
    if (isIdentifierStart(c)) {
        return scanIdentifierOrKeyword();
    }

    // Numbers
    if (isDigit(c)) {
        return scanNumber();
    }

    // String literals
    if (c == '\'' || c == '"') {
        return scanString(c);
    }

    // Backtick identifiers
    if (c == '`') {
        return scanBacktickIdentifier();
    }

    // User variables: @var
    if (c == '@') {
        if (peek_char_ahead(1) == '@') {
            return scanSystemVariable();
        }
        return scanUserVariable();
    }

    // Operators and punctuation
    return scanOperator();
}

Token Lexer::scanIdentifierOrKeyword() {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;

    while (!isAtEnd() && isIdentifierContinue(peek_char())) {
        advance();
    }

    std::string_view text = input_.substr(start_pos, current_ - start_pos);
    uint32_t length = static_cast<uint32_t>(current_ - start_pos);

    // Check for keyword (case-insensitive)
    std::string upper;
    upper.reserve(text.size());
    for (char c : text) {
        upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }

    TokenType kw_type = lookupKeyword(upper);
    if (kw_type != TokenType::IDENTIFIER) {
        return Token::makeKeyword(start_loc, length, kw_type);
    }

    // Regular identifier
    uint32_t id = string_pool_.intern(text);
    return Token::makeIdentifier(start_loc, length, id);
}

TokenType Lexer::lookupKeyword(std::string_view identifier) const {
    auto it = keywords_.find(identifier);
    if (it != keywords_.end()) {
        return it->second;
    }
    return TokenType::IDENTIFIER;
}

Token Lexer::scanNumber() {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;
    bool is_float = false;

    // Integer part
    while (!isAtEnd() && isDigit(peek_char())) {
        advance();
    }

    // Decimal part
    if (peek_char() == '.' && isDigit(peek_char_ahead(1))) {
        is_float = true;
        advance(); // '.'
        while (!isAtEnd() && isDigit(peek_char())) {
            advance();
        }
    }

    // Exponent part
    if (peek_char() == 'e' || peek_char() == 'E') {
        is_float = true;
        advance(); // 'e' or 'E'
        if (peek_char() == '+' || peek_char() == '-') {
            advance();
        }
        while (!isAtEnd() && isDigit(peek_char())) {
            advance();
        }
    }

    std::string_view text = input_.substr(start_pos, current_ - start_pos);
    uint32_t length = static_cast<uint32_t>(current_ - start_pos);

    if (is_float) {
        double value = 0.0;
        std::from_chars(text.data(), text.data() + text.size(), value);
        return Token::makeFloat(start_loc, length, value);
    } else {
        int64_t value = 0;
        std::from_chars(text.data(), text.data() + text.size(), value);
        return Token::makeInteger(start_loc, length, value);
    }
}

Token Lexer::scanString(char quote) {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;

    advance(); // Opening quote

    std::string value;
    while (!isAtEnd() && peek_char() != quote) {
        if (peek_char() == '\\') {
            advance(); // backslash
            if (!isAtEnd()) {
                char escaped = advance();
                switch (escaped) {
                    case 'n': value += '\n'; break;
                    case 'r': value += '\r'; break;
                    case 't': value += '\t'; break;
                    case '0': value += '\0'; break;
                    case '\\': value += '\\'; break;
                    case '\'': value += '\''; break;
                    case '"': value += '"'; break;
                    default: value += escaped; break;
                }
            }
        } else if (peek_char() == quote && peek_char_ahead(1) == quote) {
            // Doubled quote is escape
            value += quote;
            advance();
            advance();
        } else {
            value += advance();
        }
    }

    if (isAtEnd()) {
        return makeError("Unterminated string literal");
    }

    advance(); // Closing quote

    uint32_t length = static_cast<uint32_t>(current_ - start_pos);
    uint32_t id = string_pool_.intern(value);
    return Token::makeString(start_loc, length, id);
}

Token Lexer::scanBacktickIdentifier() {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;

    advance(); // Opening backtick

    std::string value;
    while (!isAtEnd() && peek_char() != '`') {
        if (peek_char() == '`' && peek_char_ahead(1) == '`') {
            // Doubled backtick
            value += '`';
            advance();
            advance();
        } else {
            value += advance();
        }
    }

    if (isAtEnd()) {
        return makeError("Unterminated backtick identifier");
    }

    advance(); // Closing backtick

    uint32_t length = static_cast<uint32_t>(current_ - start_pos);
    uint32_t id = string_pool_.intern(value);
    return Token::makeBacktickIdentifier(start_loc, length, id);
}

Token Lexer::scanHexLiteral() {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;

    if (peek_char() == '0') {
        // 0xABCD format
        advance(); // '0'
        advance(); // 'x' or 'X'

        int64_t value = 0;
        while (!isAtEnd() && isHexDigit(peek_char())) {
            char c = advance();
            int digit;
            if (c >= '0' && c <= '9') {
                digit = c - '0';
            } else if (c >= 'a' && c <= 'f') {
                digit = c - 'a' + 10;
            } else {
                digit = c - 'A' + 10;
            }
            value = (value << 4) | digit;
        }

        uint32_t length = static_cast<uint32_t>(current_ - start_pos);
        Token t;
        t.type = TokenType::HEX_LITERAL;
        t.span = SourceSpan(start_loc, length);
        t.value.int_value = value;
        return t;
    } else {
        // X'ABCD' format
        advance(); // 'X' or 'x'
        advance(); // opening quote

        int64_t value = 0;
        while (!isAtEnd() && peek_char() != '\'') {
            if (isHexDigit(peek_char())) {
                char c = advance();
                int digit;
                if (c >= '0' && c <= '9') {
                    digit = c - '0';
                } else if (c >= 'a' && c <= 'f') {
                    digit = c - 'a' + 10;
                } else {
                    digit = c - 'A' + 10;
                }
                value = (value << 4) | digit;
            } else {
                return makeError("Invalid hex digit in hex literal");
            }
        }

        if (isAtEnd()) {
            return makeError("Unterminated hex literal");
        }
        advance(); // closing quote

        uint32_t length = static_cast<uint32_t>(current_ - start_pos);
        Token t;
        t.type = TokenType::HEX_LITERAL;
        t.span = SourceSpan(start_loc, length);
        t.value.int_value = value;
        return t;
    }
}

Token Lexer::scanBitLiteral() {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;

    advance(); // 'b' or 'B'
    advance(); // opening quote

    int64_t value = 0;
    while (!isAtEnd() && peek_char() != '\'') {
        char c = peek_char();
        if (c == '0' || c == '1') {
            value = (value << 1) | (c - '0');
            advance();
        } else {
            return makeError("Invalid digit in bit literal");
        }
    }

    if (isAtEnd()) {
        return makeError("Unterminated bit literal");
    }
    advance(); // closing quote

    uint32_t length = static_cast<uint32_t>(current_ - start_pos);
    Token t;
    t.type = TokenType::BIT_LITERAL;
    t.span = SourceSpan(start_loc, length);
    t.value.int_value = value;
    return t;
}

Token Lexer::scanUserVariable() {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;

    advance(); // '@'

    // Variable name can be quoted or unquoted
    if (peek_char() == '`' || peek_char() == '\'' || peek_char() == '"') {
        char quote = advance();
        std::string name;
        while (!isAtEnd() && peek_char() != quote) {
            name += advance();
        }
        if (!isAtEnd()) {
            advance(); // closing quote
        }
        uint32_t length = static_cast<uint32_t>(current_ - start_pos);
        uint32_t id = string_pool_.intern(name);
        Token t;
        t.type = TokenType::USER_VARIABLE;
        t.span = SourceSpan(start_loc, length);
        t.value.string_id = id;
        return t;
    } else {
        // Unquoted variable name
        std::string name;
        while (!isAtEnd() && (isIdentifierContinue(peek_char()) || peek_char() == '.')) {
            name += advance();
        }
        uint32_t length = static_cast<uint32_t>(current_ - start_pos);
        uint32_t id = string_pool_.intern(name);
        Token t;
        t.type = TokenType::USER_VARIABLE;
        t.span = SourceSpan(start_loc, length);
        t.value.string_id = id;
        return t;
    }
}

Token Lexer::scanSystemVariable() {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;

    advance(); // first '@'
    advance(); // second '@'

    // Optional GLOBAL. or SESSION. prefix
    std::string name;
    while (!isAtEnd() && (isIdentifierContinue(peek_char()) || peek_char() == '.')) {
        name += advance();
    }

    uint32_t length = static_cast<uint32_t>(current_ - start_pos);
    uint32_t id = string_pool_.intern(name);
    Token t;
    t.type = TokenType::SYSTEM_VARIABLE;
    t.span = SourceSpan(start_loc, length);
    t.value.string_id = id;
    return t;
}

Token Lexer::scanOperator() {
    SourceLocation start_loc = location_;
    char c = advance();

    auto makeOp = [&](TokenType type, uint32_t len = 1) {
        return Token::makeOperator(start_loc, len, type);
    };

    switch (c) {
        case '(':
            return makeOp(TokenType::LEFT_PAREN);
        case ')':
            return makeOp(TokenType::RIGHT_PAREN);
        case '[':
            return makeOp(TokenType::LEFT_BRACKET);
        case ']':
            return makeOp(TokenType::RIGHT_BRACKET);
        case '{':
            return makeOp(TokenType::LEFT_BRACE);
        case '}':
            return makeOp(TokenType::RIGHT_BRACE);
        case ',':
            return makeOp(TokenType::COMMA);
        case ';':
            return makeOp(TokenType::SEMICOLON);
        case '.':
            return makeOp(TokenType::DOT);
        case '+':
            return makeOp(TokenType::PLUS);
        case '*':
            return makeOp(TokenType::STAR);
        case '/':
            return makeOp(TokenType::SLASH);
        case '%':
            return makeOp(TokenType::PERCENT);
        case '~':
            return makeOp(TokenType::TILDE);
        case '^':
            return makeOp(TokenType::CARET);
        case '?':
            return makeOp(TokenType::PLACEHOLDER);

        case '-':
            if (match('>')) {
                if (match('>')) {
                    return makeOp(TokenType::DOUBLE_ARROW, 3);
                }
                return makeOp(TokenType::ARROW, 2);
            }
            return makeOp(TokenType::MINUS);

        case '=':
            return makeOp(TokenType::EQUAL);

        case '!':
            if (match('=')) {
                return makeOp(TokenType::NOT_EQUAL, 2);
            }
            return makeOp(TokenType::NOT_OP);

        case '<':
            if (match('=')) {
                if (match('>')) {
                    return makeOp(TokenType::NULL_SAFE_EQUAL, 3);
                }
                return makeOp(TokenType::LESS_EQUAL, 2);
            }
            if (match('>')) {
                return makeOp(TokenType::NOT_EQUAL, 2);
            }
            if (match('<')) {
                return makeOp(TokenType::SHIFT_LEFT, 2);
            }
            return makeOp(TokenType::LESS_THAN);

        case '>':
            if (match('=')) {
                return makeOp(TokenType::GREATER_EQUAL, 2);
            }
            if (match('>')) {
                return makeOp(TokenType::SHIFT_RIGHT, 2);
            }
            return makeOp(TokenType::GREATER_THAN);

        case '&':
            if (match('&')) {
                return makeOp(TokenType::AND_OP, 2);
            }
            return makeOp(TokenType::AMPERSAND);

        case '|':
            if (match('|')) {
                return makeOp(TokenType::OR_OP, 2);
            }
            return makeOp(TokenType::PIPE);

        case ':':
            if (match('=')) {
                return makeOp(TokenType::COLON_EQUAL, 2);
            }
            return makeOp(TokenType::COLON);

        default:
            return makeError(std::string("Unexpected character: ") + c);
    }
}

Token Lexer::makeError(const std::string& message) {
    // Intern the error message
    uint32_t id = string_pool_.intern(message);
    Token t;
    t.type = TokenType::ERROR;
    t.span = SourceSpan(location_, 1);
    t.value.string_id = id;
    return t;
}

// ============================================================================
// Token Helper Functions
// ============================================================================

const char* tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::END_OF_FILE: return "EOF";
        case TokenType::ERROR: return "ERROR";
        case TokenType::INTEGER_LITERAL: return "INTEGER";
        case TokenType::HEX_LITERAL: return "HEX";
        case TokenType::FLOAT_LITERAL: return "FLOAT";
        case TokenType::STRING_LITERAL: return "STRING";
        case TokenType::BIT_LITERAL: return "BIT";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::BACKTICK_IDENTIFIER: return "BACKTICK_ID";
        case TokenType::PLACEHOLDER: return "PLACEHOLDER";
        case TokenType::USER_VARIABLE: return "USER_VAR";
        case TokenType::SYSTEM_VARIABLE: return "SYS_VAR";
        case TokenType::PLUS: return "+";
        case TokenType::MINUS: return "-";
        case TokenType::STAR: return "*";
        case TokenType::SLASH: return "/";
        case TokenType::DIV: return "DIV";
        case TokenType::PERCENT: return "%";
        case TokenType::EQUAL: return "=";
        case TokenType::NOT_EQUAL: return "<>";
        case TokenType::NULL_SAFE_EQUAL: return "<=>";
        case TokenType::LESS_THAN: return "<";
        case TokenType::GREATER_THAN: return ">";
        case TokenType::LESS_EQUAL: return "<=";
        case TokenType::GREATER_EQUAL: return ">=";
        case TokenType::LEFT_PAREN: return "(";
        case TokenType::RIGHT_PAREN: return ")";
        case TokenType::LEFT_BRACKET: return "[";
        case TokenType::RIGHT_BRACKET: return "]";
        case TokenType::LEFT_BRACE: return "{";
        case TokenType::RIGHT_BRACE: return "}";
        case TokenType::COMMA: return ",";
        case TokenType::SEMICOLON: return ";";
        case TokenType::DOT: return ".";
        case TokenType::COLON: return ":";
        case TokenType::KW_SELECT: return "SELECT";
        case TokenType::KW_FROM: return "FROM";
        case TokenType::KW_WHERE: return "WHERE";
        case TokenType::KW_AND: return "AND";
        case TokenType::KW_OR: return "OR";
        case TokenType::KW_NOT: return "NOT";
        case TokenType::KW_NULL: return "NULL";
        case TokenType::KW_TRUE: return "TRUE";
        case TokenType::KW_FALSE: return "FALSE";
        default: return "TOKEN";
    }
}

bool isReservedKeyword(TokenType type) {
    return type >= TokenType::KW_SELECT && type < TokenType::TOKEN_TYPE_COUNT;
}

bool isOperator(TokenType type) {
    return type >= TokenType::PLUS && type <= TokenType::COLON_EQUAL;
}

bool isPunctuation(TokenType type) {
    return type >= TokenType::LEFT_PAREN && type <= TokenType::COLON;
}

bool isTypeKeyword(TokenType type) {
    return type >= TokenType::KW_TINYINT && type <= TokenType::KW_POLYGON;
}

} // namespace scratchbird::parser::mysql
