/**
 * PostgreSQL Lexer Implementation
 *
 * Tokenizes PostgreSQL 16 SQL syntax.
 */

#include "scratchbird/parser/postgresql/pg_lexer.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <stdexcept>
#include <charconv>
#include <limits>

namespace scratchbird::parser::postgresql {

// ============================================================================
// StringPool Implementation
// ============================================================================

uint32_t StringPool::intern(std::string_view str) {
    std::string key(str);
    auto it = index_.find(key);
    if (it != index_.end()) {
        return it->second;
    }
    uint32_t id = static_cast<uint32_t>(strings_.size());
    strings_.push_back(key);
    index_[key] = id;
    return id;
}

std::string_view StringPool::get(uint32_t id) const {
    if (id >= strings_.size()) {
        return {};
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
        {"select", TokenType::KW_SELECT},
        {"insert", TokenType::KW_INSERT},
        {"update", TokenType::KW_UPDATE},
        {"delete", TokenType::KW_DELETE},
        {"create", TokenType::KW_CREATE},
        {"alter", TokenType::KW_ALTER},
        {"drop", TokenType::KW_DROP},
        {"truncate", TokenType::KW_TRUNCATE},
        {"grant", TokenType::KW_GRANT},
        {"revoke", TokenType::KW_REVOKE},
        {"merge", TokenType::KW_MERGE},
        {"analyze", TokenType::KW_ANALYZE},
        {"explain", TokenType::KW_EXPLAIN},
        {"call", TokenType::KW_CALL},
        {"do", TokenType::KW_DO},

        // Clause keywords
        {"from", TokenType::KW_FROM},
        {"where", TokenType::KW_WHERE},
        {"group", TokenType::KW_GROUP},
        {"having", TokenType::KW_HAVING},
        {"order", TokenType::KW_ORDER},
        {"by", TokenType::KW_BY},
        {"limit", TokenType::KW_LIMIT},
        {"offset", TokenType::KW_OFFSET},
        {"fetch", TokenType::KW_FETCH},
        {"union", TokenType::KW_UNION},
        {"intersect", TokenType::KW_INTERSECT},
        {"except", TokenType::KW_EXCEPT},
        {"with", TokenType::KW_WITH},
        {"recursive", TokenType::KW_RECURSIVE},

        // Join keywords
        {"join", TokenType::KW_JOIN},
        {"inner", TokenType::KW_INNER},
        {"left", TokenType::KW_LEFT},
        {"right", TokenType::KW_RIGHT},
        {"full", TokenType::KW_FULL},
        {"cross", TokenType::KW_CROSS},
        {"natural", TokenType::KW_NATURAL},
        {"outer", TokenType::KW_OUTER},
        {"on", TokenType::KW_ON},
        {"using", TokenType::KW_USING},
        {"lateral", TokenType::KW_LATERAL},

        // Expression keywords
        {"and", TokenType::KW_AND},
        {"or", TokenType::KW_OR},
        {"not", TokenType::KW_NOT},
        {"is", TokenType::KW_IS},
        {"in", TokenType::KW_IN},
        {"between", TokenType::KW_BETWEEN},
        {"like", TokenType::KW_LIKE},
        {"ilike", TokenType::KW_ILIKE},
        {"similar", TokenType::KW_SIMILAR},
        {"case", TokenType::KW_CASE},
        {"when", TokenType::KW_WHEN},
        {"then", TokenType::KW_THEN},
        {"else", TokenType::KW_ELSE},
        {"end", TokenType::KW_END},
        {"null", TokenType::KW_NULL},
        {"true", TokenType::KW_TRUE},
        {"false", TokenType::KW_FALSE},
        {"exists", TokenType::KW_EXISTS},
        {"cast", TokenType::KW_CAST},
        {"as", TokenType::KW_AS},
        {"any", TokenType::KW_ANY},
        {"some", TokenType::KW_SOME},
        {"all", TokenType::KW_ALL},

        // DML keywords
        {"into", TokenType::KW_INTO},
        {"values", TokenType::KW_VALUES},
        {"default", TokenType::KW_DEFAULT},
        {"returning", TokenType::KW_RETURNING},
        {"conflict", TokenType::KW_CONFLICT},
        {"nothing", TokenType::KW_NOTHING},
        {"set", TokenType::KW_SET},
        {"reset", TokenType::KW_RESET},
        {"only", TokenType::KW_ONLY},

        // DDL keywords
        {"table", TokenType::KW_TABLE},
        {"database", TokenType::KW_DATABASE},
        {"schema", TokenType::KW_SCHEMA},
        {"index", TokenType::KW_INDEX},
        {"view", TokenType::KW_VIEW},
        {"materialized", TokenType::KW_MATERIALIZED},
        {"sequence", TokenType::KW_SEQUENCE},
        {"function", TokenType::KW_FUNCTION},
        {"procedure", TokenType::KW_PROCEDURE},
        {"trigger", TokenType::KW_TRIGGER},
        {"type", TokenType::KW_TYPE},
        {"types", TokenType::KW_TYPES},
        {"domain", TokenType::KW_DOMAIN},
        {"constraint", TokenType::KW_CONSTRAINT},
        {"column", TokenType::KW_COLUMN},
        {"add", TokenType::KW_ADD},
        {"rename", TokenType::KW_RENAME},
        {"to", TokenType::KW_TO},
        {"if", TokenType::KW_IF},
        {"temporary", TokenType::KW_TEMPORARY},
        {"temp", TokenType::KW_TEMP},
        {"unlogged", TokenType::KW_UNLOGGED},
        {"unique", TokenType::KW_UNIQUE},
        {"primary", TokenType::KW_PRIMARY},
        {"key", TokenType::KW_KEY},
        {"foreign", TokenType::KW_FOREIGN},
        {"references", TokenType::KW_REFERENCES},
        {"check", TokenType::KW_CHECK},
        {"cascade", TokenType::KW_CASCADE},
        {"restrict", TokenType::KW_RESTRICT},
        {"location", TokenType::KW_LOCATION},
        {"unlimited", TokenType::KW_UNLIMITED},
        {"no", TokenType::KW_NO},
        {"action", TokenType::KW_ACTION},
        {"initially", TokenType::KW_INITIALLY},
        {"deferred", TokenType::KW_DEFERRED},
        {"immediate", TokenType::KW_IMMEDIATE},
        {"deferrable", TokenType::KW_DEFERRABLE},
        {"concurrently", TokenType::KW_CONCURRENTLY},
        {"include", TokenType::KW_INCLUDE},
        {"nulls", TokenType::KW_NULLS},
        {"inherits", TokenType::KW_INHERITS},
        {"partition", TokenType::KW_PARTITION},
        {"range", TokenType::KW_RANGE},
        {"list", TokenType::KW_LIST},
        {"hash", TokenType::KW_HASH},
        {"for", TokenType::KW_FOR},
        {"collate", TokenType::KW_COLLATE},
        {"generated", TokenType::KW_GENERATED},
        {"always", TokenType::KW_ALWAYS},
        {"identity", TokenType::KW_IDENTITY},
        {"stored", TokenType::KW_STORED},
        {"replace", TokenType::KW_REPLACE},

        // Type keywords
        {"smallint", TokenType::KW_SMALLINT},
        {"int", TokenType::KW_INT},
        {"integer", TokenType::KW_INTEGER},
        {"bigint", TokenType::KW_BIGINT},
        {"int128", TokenType::KW_INT128},
        {"uint128", TokenType::KW_UINT128},
        {"real", TokenType::KW_REAL},
        {"double", TokenType::KW_DOUBLE},
        {"precision", TokenType::KW_PRECISION},
        {"decimal", TokenType::KW_DECIMAL},
        {"numeric", TokenType::KW_NUMERIC},
        {"smallserial", TokenType::KW_SMALLSERIAL},
        {"serial", TokenType::KW_SERIAL},
        {"bigserial", TokenType::KW_BIGSERIAL},
        {"money", TokenType::KW_MONEY},
        {"char", TokenType::KW_CHAR},
        {"character", TokenType::KW_CHARACTER},
        {"varchar", TokenType::KW_VARCHAR},
        {"varying", TokenType::KW_VARYING},
        {"text", TokenType::KW_TEXT},
        {"bytea", TokenType::KW_BYTEA},
        {"date", TokenType::KW_DATE},
        {"time", TokenType::KW_TIME},
        {"timestamp", TokenType::KW_TIMESTAMP},
        {"interval", TokenType::KW_INTERVAL},
        {"boolean", TokenType::KW_BOOLEAN},
        {"bool", TokenType::KW_BOOL},
        {"uuid", TokenType::KW_UUID},
        {"json", TokenType::KW_JSON},
        {"jsonb", TokenType::KW_JSONB},
        {"jsonpath", TokenType::KW_JSONPATH},
        {"xml", TokenType::KW_XML},
        {"array", TokenType::KW_ARRAY},
        {"point", TokenType::KW_POINT},
        {"line", TokenType::KW_LINE},
        {"lseg", TokenType::KW_LSEG},
        {"box", TokenType::KW_BOX},
        {"path", TokenType::KW_PATH},
        {"polygon", TokenType::KW_POLYGON},
        {"circle", TokenType::KW_CIRCLE},
        {"cidr", TokenType::KW_CIDR},
        {"inet", TokenType::KW_INET},
        {"macaddr", TokenType::KW_MACADDR},
        {"macaddr8", TokenType::KW_MACADDR8},
        {"bit", TokenType::KW_BIT},
        {"tsvector", TokenType::KW_TSVECTOR},
        {"tsquery", TokenType::KW_TSQUERY},
        {"int4range", TokenType::KW_INT4RANGE},
        {"int8range", TokenType::KW_INT8RANGE},
        {"numrange", TokenType::KW_NUMRANGE},
        {"daterange", TokenType::KW_DATERANGE},
        {"tsrange", TokenType::KW_TSRANGE},
        {"tstzrange", TokenType::KW_TSTZRANGE},
        {"oid", TokenType::KW_OID},
        {"regclass", TokenType::KW_REGCLASS},
        {"regtype", TokenType::KW_REGTYPE},
        {"without", TokenType::KW_WITHOUT},
        {"zone", TokenType::KW_ZONE},

        // Aggregate/Window keywords
        {"distinct", TokenType::KW_DISTINCT},
        {"asc", TokenType::KW_ASC},
        {"desc", TokenType::KW_DESC},
        {"first", TokenType::KW_FIRST},
        {"last", TokenType::KW_LAST},
        {"rollup", TokenType::KW_ROLLUP},
        {"cube", TokenType::KW_CUBE},
        {"grouping", TokenType::KW_GROUPING},
        {"sets", TokenType::KW_SETS},
        {"over", TokenType::KW_OVER},
        {"window", TokenType::KW_WINDOW},
        {"rows", TokenType::KW_ROWS},
        {"groups", TokenType::KW_GROUPS},
        {"unbounded", TokenType::KW_UNBOUNDED},
        {"preceding", TokenType::KW_PRECEDING},
        {"following", TokenType::KW_FOLLOWING},
        {"current", TokenType::KW_CURRENT},
        {"row", TokenType::KW_ROW},
        {"ties", TokenType::KW_TIES},
        {"exclude", TokenType::KW_EXCLUDE},

        // Transaction keywords
        {"begin", TokenType::KW_BEGIN},
        {"start", TokenType::KW_START},
        {"transaction", TokenType::KW_TRANSACTION},
        {"commit", TokenType::KW_COMMIT},
        {"rollback", TokenType::KW_ROLLBACK},
        {"savepoint", TokenType::KW_SAVEPOINT},
        {"release", TokenType::KW_RELEASE},
        {"work", TokenType::KW_WORK},
        {"read", TokenType::KW_READ},
        {"write", TokenType::KW_WRITE},
        {"committed", TokenType::KW_COMMITTED},
        {"uncommitted", TokenType::KW_UNCOMMITTED},
        {"repeatable", TokenType::KW_REPEATABLE},
        {"serializable", TokenType::KW_SERIALIZABLE},
        {"isolation", TokenType::KW_ISOLATION},
        {"level", TokenType::KW_LEVEL},

        // PL/pgSQL keywords
        {"declare", TokenType::KW_DECLARE},
        {"return", TokenType::KW_RETURN},
        {"returns", TokenType::KW_RETURNS},
        {"language", TokenType::KW_LANGUAGE},
        {"plpgsql", TokenType::KW_PLPGSQL},
        {"sql", TokenType::KW_SQL},
        {"immutable", TokenType::KW_IMMUTABLE},
        {"stable", TokenType::KW_STABLE},
        {"volatile", TokenType::KW_VOLATILE},
        {"strict", TokenType::KW_STRICT},
        {"called", TokenType::KW_CALLED},
        {"input", TokenType::KW_INPUT},
        {"security", TokenType::KW_SECURITY},
        {"definer", TokenType::KW_DEFINER},
        {"invoker", TokenType::KW_INVOKER},
        {"external", TokenType::KW_EXTERNAL},
        {"cost", TokenType::KW_COST},
        {"parallel", TokenType::KW_PARALLEL},
        {"safe", TokenType::KW_SAFE},
        {"restricted", TokenType::KW_RESTRICTED},
        {"unsafe", TokenType::KW_UNSAFE},
        {"leakproof", TokenType::KW_LEAKPROOF},
        {"loop", TokenType::KW_LOOP},
        {"while", TokenType::KW_WHILE},
        {"exit", TokenType::KW_EXIT},
        {"continue", TokenType::KW_CONTINUE},
        {"foreach", TokenType::KW_FOREACH},
        {"slice", TokenType::KW_SLICE},
        {"raise", TokenType::KW_RAISE},
        {"notice", TokenType::KW_NOTICE},
        {"warning", TokenType::KW_WARNING},
        {"exception", TokenType::KW_EXCEPTION},
        {"debug", TokenType::KW_DEBUG},
        {"log", TokenType::KW_LOG},
        {"info", TokenType::KW_INFO},
        {"assert", TokenType::KW_ASSERT},
        {"get", TokenType::KW_GET},
        {"diagnostics", TokenType::KW_DIAGNOSTICS},
        {"stacked", TokenType::KW_STACKED},
        {"cursor", TokenType::KW_CURSOR},
        {"scroll", TokenType::KW_SCROLL},
        {"hold", TokenType::KW_HOLD},
        {"move", TokenType::KW_MOVE},
        {"close", TokenType::KW_CLOSE},
        {"open", TokenType::KW_OPEN},
        {"show", TokenType::KW_SHOW},

        // Trigger keywords
        {"before", TokenType::KW_BEFORE},
        {"after", TokenType::KW_AFTER},
        {"instead", TokenType::KW_INSTEAD},
        {"of", TokenType::KW_OF},
        {"each", TokenType::KW_EACH},
        {"statement", TokenType::KW_STATEMENT},
        {"execute", TokenType::KW_EXECUTE},
        {"new", TokenType::KW_NEW},
        {"old", TokenType::KW_OLD},
        {"referencing", TokenType::KW_REFERENCING},

        // Security keywords
        {"user", TokenType::KW_USER},
        {"role", TokenType::KW_ROLE},
        {"policy", TokenType::KW_POLICY},
        {"group", TokenType::KW_GROUP},
        {"public", TokenType::KW_PUBLIC},
        {"privileges", TokenType::KW_PRIVILEGES},
        {"option", TokenType::KW_OPTION},
        {"admin", TokenType::KW_ADMIN},
        {"password", TokenType::KW_PASSWORD},
        {"superuser", TokenType::KW_SUPERUSER},
        {"nosuperuser", TokenType::KW_NOSUPERUSER},
        {"createdb", TokenType::KW_CREATEDB},
        {"nocreatedb", TokenType::KW_NOCREATEDB},
        {"createrole", TokenType::KW_CREATEROLE},
        {"nocreaterole", TokenType::KW_NOCREATEROLE},
        {"login", TokenType::KW_LOGIN},
        {"nologin", TokenType::KW_NOLOGIN},
        {"replication", TokenType::KW_REPLICATION},
        {"noreplication", TokenType::KW_NOREPLICATION},
        {"inherit", TokenType::KW_INHERIT},
        {"noinherit", TokenType::KW_NOINHERIT},
        {"bypassrls", TokenType::KW_BYPASSRLS},
        {"nobypassrls", TokenType::KW_NOBYPASSRLS},
        {"valid", TokenType::KW_VALID},
        {"until", TokenType::KW_UNTIL},
        {"connection", TokenType::KW_CONNECTION},

        // System/Session keywords
        {"local", TokenType::KW_LOCAL},
        {"session", TokenType::KW_SESSION},
        {"current_user", TokenType::KW_CURRENT_USER},
        {"session_user", TokenType::KW_SESSION_USER},
        {"current_role", TokenType::KW_CURRENT_ROLE},
        {"current_schema", TokenType::KW_CURRENT_SCHEMA},
        {"current_catalog", TokenType::KW_CURRENT_CATALOG},
        {"current_date", TokenType::KW_CURRENT_DATE},
        {"current_time", TokenType::KW_CURRENT_TIME},
        {"current_timestamp", TokenType::KW_CURRENT_TIMESTAMP},
        {"localtime", TokenType::KW_LOCALTIME},
        {"localtimestamp", TokenType::KW_LOCALTIMESTAMP},

        // Misc keywords
        {"verbose", TokenType::KW_VERBOSE},
        {"force", TokenType::KW_FORCE},
        {"enable", TokenType::KW_ENABLE},
        {"disable", TokenType::KW_DISABLE},
        {"match", TokenType::KW_MATCH},
        {"partial", TokenType::KW_PARTIAL},
        {"simple", TokenType::KW_SIMPLE},
        {"owner", TokenType::KW_OWNER},
        {"owned", TokenType::KW_OWNED},
        {"none", TokenType::KW_NONE},
        {"extract", TokenType::KW_EXTRACT},
        {"alter_element", TokenType::KW_ALTER_ELEMENT},
        {"position", TokenType::KW_POSITION},
        {"substring", TokenType::KW_SUBSTRING},
        {"trim", TokenType::KW_TRIM},
        {"leading", TokenType::KW_LEADING},
        {"trailing", TokenType::KW_TRAILING},
        {"both", TokenType::KW_BOTH},
        {"coalesce", TokenType::KW_COALESCE},
        {"nullif", TokenType::KW_NULLIF},
        {"greatest", TokenType::KW_GREATEST},
        {"least", TokenType::KW_LEAST},
        {"normalize", TokenType::KW_NORMALIZE},
        {"normalized", TokenType::KW_NORMALIZED},
        {"nfc", TokenType::KW_NFC},
        {"nfd", TokenType::KW_NFD},
        {"nfkc", TokenType::KW_NFKC},
        {"nfkd", TokenType::KW_NFKD},
        {"overlaps", TokenType::KW_OVERLAPS},
        {"placing", TokenType::KW_PLACING},
        {"symmetric", TokenType::KW_SYMMETRIC},
        {"asymmetric", TokenType::KW_ASYMMETRIC},
        {"freeze", TokenType::KW_FREEZE},
        {"tablesample", TokenType::KW_TABLESAMPLE},
        {"within", TokenType::KW_WITHIN},
        {"variadic", TokenType::KW_VARIADIC},

        // Copy keywords
        {"copy", TokenType::KW_COPY},
        {"stdin", TokenType::KW_STDIN},
        {"stdout", TokenType::KW_STDOUT},
        {"delimiter", TokenType::KW_DELIMITER},
        {"csv", TokenType::KW_CSV},
        {"header", TokenType::KW_HEADER},
        {"quote", TokenType::KW_QUOTE},
        {"escape", TokenType::KW_ESCAPE},
        {"encoding", TokenType::KW_ENCODING},
        {"format", TokenType::KW_FORMAT},

        // Enum keyword
        {"enum", TokenType::KW_ENUM},
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
{
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
    location_.offset = current_;
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
    if (current_ >= input_.size() || input_[current_] != expected) {
        return false;
    }
    advance();
    return true;
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek_char();
        if (isWhitespace(c)) {
            advance();
        } else if (c == '-' && peek_char_ahead(1) == '-') {
            skipLineComment();
        } else if (c == '/' && peek_char_ahead(1) == '*') {
            skipBlockComment();
        } else {
            break;
        }
    }
}

void Lexer::skipLineComment() {
    // Skip --
    advance();
    advance();
    while (!isAtEnd() && peek_char() != '\n') {
        advance();
    }
    if (!isAtEnd()) {
        advance();  // Skip newline
    }
}

void Lexer::skipBlockComment() {
    // Skip /*
    advance();
    advance();
    int depth = 1;
    while (!isAtEnd() && depth > 0) {
        char c = peek_char();
        if (c == '/' && peek_char_ahead(1) == '*') {
            advance();
            advance();
            depth++;  // PostgreSQL supports nested block comments
        } else if (c == '*' && peek_char_ahead(1) == '/') {
            advance();
            advance();
            depth--;
        } else {
            advance();
        }
    }
}

Token Lexer::peek() {
    if (!has_peeked_) {
        peeked_token_ = nextToken();
        has_peeked_ = true;
    }
    return peeked_token_;
}

Token Lexer::nextToken() {
    if (has_peeked_) {
        has_peeked_ = false;
        return peeked_token_;
    }
    return scanToken();
}

Token Lexer::scanToken() {
    skipWhitespace();

    if (isAtEnd()) {
        return Token::makeEOF(location_);
    }

    SourceLocation start_loc = location_;
    char c = peek_char();

    // Check for escape string E'...'
    if ((c == 'E' || c == 'e') && peek_char_ahead(1) == '\'') {
        return scanEscapeString();
    }

    // Check for bit string B'...'
    if ((c == 'B' || c == 'b') && peek_char_ahead(1) == '\'') {
        return scanBitString();
    }

    // Check for hex string X'...'
    if ((c == 'X' || c == 'x') && peek_char_ahead(1) == '\'') {
        return scanHexString();
    }

    // Check for dollar-quoted string
    if (c == '$') {
        // Could be dollar-quoted string or parameter
        if (isDigit(peek_char_ahead(1))) {
            return scanParameter();
        }
        // Check if it's start of dollar-quoting: $$ or $tag$
        return scanDollarString();
    }

    // Identifiers and keywords
    if (isIdentifierStart(c)) {
        return scanIdentifierOrKeyword();
    }

    // Numbers
    if (isDigit(c)) {
        return scanNumber();
    }

    // Strings
    if (c == '\'') {
        return scanString('\'');
    }

    // Double-quoted identifiers
    if (c == '"') {
        return scanQuotedIdentifier();
    }

    // Operators and punctuation
    return scanOperator();
}

Token Lexer::scanIdentifierOrKeyword() {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;

    advance();  // First character
    while (!isAtEnd() && isIdentifierContinue(peek_char())) {
        advance();
    }

    size_t len = current_ - start_pos;
    std::string_view text = input_.substr(start_pos, len);

    // Convert to lowercase for keyword lookup (PostgreSQL is case-insensitive)
    std::string lower_text;
    lower_text.reserve(len);
    for (char c : text) {
        lower_text += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    // Check if it's a keyword
    TokenType kwType = lookupKeyword(lower_text);
    if (kwType != TokenType::IDENTIFIER) {
        return Token::makeKeyword(start_loc, static_cast<uint32_t>(len), kwType);
    }

    // It's an identifier - intern the lowercase version
    uint32_t id = string_pool_.intern(lower_text);
    return Token::makeIdentifier(start_loc, static_cast<uint32_t>(len), id);
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

    if (peek_char() == '0' && (peek_char_ahead(1) == 'x' || peek_char_ahead(1) == 'X')) {
        advance();  // 0
        advance();  // x
        if (!isHexDigit(peek_char())) {
            return makeError("Invalid integer number");
        }
        while (!isAtEnd() && isHexDigit(peek_char())) {
            advance();
        }
        size_t len = current_ - start_pos;
        std::string_view text = input_.substr(start_pos, len);
        uint64_t value = 0;
        const char* begin = text.data() + 2;
        const char* end = text.data() + text.size();
        auto result = std::from_chars(begin, end, value, 16);
        if (result.ec != std::errc() || result.ptr != end) {
            return makeError("Invalid integer number");
        }
        if (value > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            return makeError("Integer literal out of range");
        }
        return Token::makeInteger(start_loc, static_cast<uint32_t>(len),
                                  static_cast<int64_t>(value));
    }

    // Integer part
    while (!isAtEnd() && isDigit(peek_char())) {
        advance();
    }

    // Check for decimal point
    if (peek_char() == '.' && isDigit(peek_char_ahead(1))) {
        is_float = true;
        advance();  // Skip '.'
        while (!isAtEnd() && isDigit(peek_char())) {
            advance();
        }
    }

    // Check for exponent
    if (peek_char() == 'e' || peek_char() == 'E') {
        is_float = true;
        advance();
        if (peek_char() == '+' || peek_char() == '-') {
            advance();
        }
        while (!isAtEnd() && isDigit(peek_char())) {
            advance();
        }
    }

    size_t len = current_ - start_pos;
    std::string_view text = input_.substr(start_pos, len);

    if (is_float) {
        double val = std::stod(std::string(text));
        return Token::makeFloat(start_loc, static_cast<uint32_t>(len), val);
    } else {
        int64_t val = std::stoll(std::string(text));
        return Token::makeInteger(start_loc, static_cast<uint32_t>(len), val);
    }
}

Token Lexer::scanString(char quote) {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;

    advance();  // Skip opening quote

    std::string value;
    while (!isAtEnd()) {
        char c = peek_char();
        if (c == quote) {
            if (peek_char_ahead(1) == quote) {
                // Escaped quote
                advance();
                advance();
                value += quote;
            } else {
                // End of string
                break;
            }
        } else {
            value += c;
            advance();
        }
    }

    if (isAtEnd()) {
        return makeError("Unterminated string literal");
    }

    advance();  // Skip closing quote

    size_t len = current_ - start_pos;
    uint32_t id = string_pool_.intern(value);
    return Token::makeString(start_loc, static_cast<uint32_t>(len), id);
}

Token Lexer::scanDollarString() {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;

    advance();  // Skip first $

    // Get the tag (empty for $$, or identifier for $tag$)
    std::string tag;
    while (!isAtEnd() && peek_char() != '$') {
        char c = peek_char();
        if (!isIdentifierContinue(c)) {
            // Not a valid dollar-quote tag - this might be an error or misparse
            // Treat as single $ and let parser handle it
            return Token::makeOperator(start_loc, 1, TokenType::PARAMETER);
        }
        tag += c;
        advance();
    }

    if (isAtEnd()) {
        return makeError("Unterminated dollar-quoted string");
    }

    advance();  // Skip closing $ of opening tag

    // Now scan the content until we find the closing tag
    std::string closing_tag = "$" + tag + "$";
    std::string content;

    while (!isAtEnd()) {
        // Check if we're at the closing tag
        bool found_close = true;
        for (size_t i = 0; i < closing_tag.size(); i++) {
            if (peek_char_ahead(i) != closing_tag[i]) {
                found_close = false;
                break;
            }
        }
        if (found_close) {
            // Skip the closing tag
            for (size_t i = 0; i < closing_tag.size(); i++) {
                advance();
            }
            break;
        }
        content += peek_char();
        advance();
    }

    size_t len = current_ - start_pos;
    uint32_t id = string_pool_.intern(content);
    return Token::makeDollarString(start_loc, static_cast<uint32_t>(len), id);
}

Token Lexer::scanEscapeString() {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;

    advance();  // Skip 'E' or 'e'
    advance();  // Skip opening quote

    std::string value;
    while (!isAtEnd()) {
        char c = peek_char();
        if (c == '\'') {
            if (peek_char_ahead(1) == '\'') {
                // Escaped quote
                advance();
                advance();
                value += '\'';
            } else {
                // End of string
                break;
            }
        } else if (c == '\\') {
            advance();
            if (isAtEnd()) break;
            char esc = peek_char();
            switch (esc) {
                case 'b': value += '\b'; break;
                case 'f': value += '\f'; break;
                case 'n': value += '\n'; break;
                case 'r': value += '\r'; break;
                case 't': value += '\t'; break;
                case '\\': value += '\\'; break;
                case '\'': value += '\''; break;
                default:
                    // Octal or hex or unicode - simplified handling
                    value += esc;
                    break;
            }
            advance();
        } else {
            value += c;
            advance();
        }
    }

    if (isAtEnd()) {
        return makeError("Unterminated escape string literal");
    }

    advance();  // Skip closing quote

    size_t len = current_ - start_pos;
    uint32_t id = string_pool_.intern(value);
    return Token::makeEscapeString(start_loc, static_cast<uint32_t>(len), id);
}

Token Lexer::scanQuotedIdentifier() {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;

    advance();  // Skip opening quote

    std::string value;
    while (!isAtEnd()) {
        char c = peek_char();
        if (c == '"') {
            if (peek_char_ahead(1) == '"') {
                // Escaped quote
                advance();
                advance();
                value += '"';
            } else {
                // End of identifier
                break;
            }
        } else {
            value += c;
            advance();
        }
    }

    if (isAtEnd()) {
        return makeError("Unterminated quoted identifier");
    }

    advance();  // Skip closing quote

    size_t len = current_ - start_pos;
    uint32_t id = string_pool_.intern(value);  // Preserve case for quoted identifiers
    return Token::makeQuotedIdentifier(start_loc, static_cast<uint32_t>(len), id);
}

Token Lexer::scanBitString() {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;

    advance();  // Skip 'B' or 'b'
    advance();  // Skip opening quote

    std::string value;
    while (!isAtEnd() && peek_char() != '\'') {
        char c = peek_char();
        if (c != '0' && c != '1') {
            return makeError("Invalid character in bit string literal");
        }
        value += c;
        advance();
    }

    if (isAtEnd()) {
        return makeError("Unterminated bit string literal");
    }

    advance();  // Skip closing quote

    size_t len = current_ - start_pos;
    uint32_t id = string_pool_.intern(value);
    Token t;
    t.type = TokenType::BIT_STRING;
    t.span = SourceSpan(start_loc, static_cast<uint32_t>(len));
    t.value.string_id = id;
    return t;
}

Token Lexer::scanHexString() {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;

    advance();  // Skip 'X' or 'x'
    advance();  // Skip opening quote

    std::string value;
    while (!isAtEnd() && peek_char() != '\'') {
        char c = peek_char();
        if (!isHexDigit(c)) {
            return makeError("Invalid character in hex string literal");
        }
        value += c;
        advance();
    }

    if (isAtEnd()) {
        return makeError("Unterminated hex string literal");
    }

    advance();  // Skip closing quote

    size_t len = current_ - start_pos;
    uint32_t id = string_pool_.intern(value);
    Token t;
    t.type = TokenType::HEX_STRING;
    t.span = SourceSpan(start_loc, static_cast<uint32_t>(len));
    t.value.string_id = id;
    return t;
}

Token Lexer::scanParameter() {
    SourceLocation start_loc = location_;
    size_t start_pos = current_;

    advance();  // Skip $

    // Read parameter number
    std::string num;
    while (!isAtEnd() && isDigit(peek_char())) {
        num += peek_char();
        advance();
    }

    size_t len = current_ - start_pos;
    int64_t param_num = std::stoll(num);
    return Token::makeParameter(start_loc, static_cast<uint32_t>(len), param_num);
}

Token Lexer::scanOperator() {
    SourceLocation start_loc = location_;
    char c = peek_char();

    // Single-character operators and punctuation
    switch (c) {
        case '(':
            advance();
            return Token::makePunctuation(start_loc, 1, TokenType::LEFT_PAREN);
        case ')':
            advance();
            return Token::makePunctuation(start_loc, 1, TokenType::RIGHT_PAREN);
        case '[':
            advance();
            return Token::makePunctuation(start_loc, 1, TokenType::LEFT_BRACKET);
        case ']':
            advance();
            return Token::makePunctuation(start_loc, 1, TokenType::RIGHT_BRACKET);
        case '{':
            advance();
            return Token::makePunctuation(start_loc, 1, TokenType::LEFT_BRACE);
        case '}':
            advance();
            return Token::makePunctuation(start_loc, 1, TokenType::RIGHT_BRACE);
        case ',':
            advance();
            return Token::makePunctuation(start_loc, 1, TokenType::COMMA);
        case ';':
            advance();
            return Token::makePunctuation(start_loc, 1, TokenType::SEMICOLON);
        case '.':
            advance();
            return Token::makePunctuation(start_loc, 1, TokenType::DOT);
        case '+':
            advance();
            return Token::makeOperator(start_loc, 1, TokenType::PLUS);
        case '*':
            advance();
            return Token::makeOperator(start_loc, 1, TokenType::STAR);
        case '/':
            advance();
            return Token::makeOperator(start_loc, 1, TokenType::SLASH);
        case '%':
            advance();
            return Token::makeOperator(start_loc, 1, TokenType::PERCENT);
        case '^':
            advance();
            return Token::makeOperator(start_loc, 1, TokenType::CARET);
        case '~':
            advance();
            if (match('*')) {
                return Token::makeOperator(start_loc, 2, TokenType::TILDE_STAR);
            }
            return Token::makeOperator(start_loc, 1, TokenType::TILDE);
        case '=':
            advance();
            return Token::makeOperator(start_loc, 1, TokenType::EQUAL);
        case '&':
            advance();
            if (match('&')) {
                return Token::makeOperator(start_loc, 2, TokenType::DOUBLE_AMPERSAND);
            }
            return Token::makeOperator(start_loc, 1, TokenType::AMPERSAND);
        case '#':
            advance();
            if (match('>')) {
                if (match('>')) {
                    return Token::makeOperator(start_loc, 3, TokenType::HASH_DOUBLE_ARROW);
                }
                return Token::makeOperator(start_loc, 2, TokenType::HASH_ARROW);
            }
            return Token::makeOperator(start_loc, 1, TokenType::HASH);
        case '?':
            advance();
            if (match('|')) {
                return Token::makeOperator(start_loc, 2, TokenType::QUESTION_PIPE);
            }
            if (match('&')) {
                return Token::makeOperator(start_loc, 2, TokenType::QUESTION_AMPERSAND);
            }
            return Token::makeOperator(start_loc, 1, TokenType::QUESTION);
        case '@':
            advance();
            if (match('>')) {
                return Token::makeOperator(start_loc, 2, TokenType::AT_GREATER);
            }
            if (match('@')) {
                return Token::makeOperator(start_loc, 2, TokenType::AT_AT);
            }
            if (match('?')) {
                return Token::makeOperator(start_loc, 2, TokenType::AT_QUESTION);
            }
            return Token::makeOperator(start_loc, 1, TokenType::AT_SIGN);
    }

    // Multi-character operators
    if (c == '-') {
        advance();
        if (match('>')) {
            if (match('>')) {
                return Token::makeOperator(start_loc, 3, TokenType::DOUBLE_ARROW);
            }
            return Token::makeOperator(start_loc, 2, TokenType::ARROW);
        }
        if (match('|') && match('-')) {
            return Token::makeOperator(start_loc, 3, TokenType::MINUS_PIPE_MINUS);
        }
        return Token::makeOperator(start_loc, 1, TokenType::MINUS);
    }

    if (c == '<') {
        advance();
        if (match('>')) {
            return Token::makeOperator(start_loc, 2, TokenType::NOT_EQUAL);
        }
        if (match('=')) {
            return Token::makeOperator(start_loc, 2, TokenType::LESS_EQUAL);
        }
        if (match('<')) {
            return Token::makeOperator(start_loc, 2, TokenType::SHIFT_LEFT);
        }
        if (match('@')) {
            return Token::makeOperator(start_loc, 2, TokenType::LESS_AT);
        }
        return Token::makeOperator(start_loc, 1, TokenType::LESS_THAN);
    }

    if (c == '>') {
        advance();
        if (match('=')) {
            return Token::makeOperator(start_loc, 2, TokenType::GREATER_EQUAL);
        }
        if (match('>')) {
            return Token::makeOperator(start_loc, 2, TokenType::SHIFT_RIGHT);
        }
        return Token::makeOperator(start_loc, 1, TokenType::GREATER_THAN);
    }

    if (c == '|') {
        advance();
        if (match('|')) {
            if (match('/')) {
                return Token::makeOperator(start_loc, 3, TokenType::DOUBLE_PIPE_SLASH);
            }
            return Token::makeOperator(start_loc, 2, TokenType::DOUBLE_PIPE);
        }
        if (match('/')) {
            return Token::makeOperator(start_loc, 2, TokenType::PIPE_SLASH);
        }
        return Token::makeOperator(start_loc, 1, TokenType::PIPE);
    }

    if (c == '!') {
        advance();
        if (match('=')) {
            return Token::makeOperator(start_loc, 2, TokenType::NOT_EQUAL);
        }
        if (match('~')) {
            if (match('*')) {
                return Token::makeOperator(start_loc, 3, TokenType::EXCLAIM_TILDE_STAR);
            }
            return Token::makeOperator(start_loc, 2, TokenType::EXCLAIM_TILDE);
        }
        if (match('!')) {
            return Token::makeOperator(start_loc, 2, TokenType::DOUBLE_EXCLAIM);
        }
        return Token::makeOperator(start_loc, 1, TokenType::EXCLAIM);
    }

    if (c == ':') {
        advance();
        if (match(':')) {
            return Token::makeOperator(start_loc, 2, TokenType::DOUBLE_COLON);
        }
        return Token::makePunctuation(start_loc, 1, TokenType::COLON);
    }

    // Unknown character
    advance();
    return makeError("Unexpected character");
}

Token Lexer::makeError(const std::string& message) {
    Token t;
    t.type = TokenType::ERROR;
    t.span = SourceSpan(location_, 1);
    // Store error message in string pool
    t.value.string_id = string_pool_.intern(message);
    return t;
}

// ============================================================================
// Token Type Helpers
// ============================================================================

const char* tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::END_OF_FILE: return "END_OF_FILE";
        case TokenType::ERROR: return "ERROR";
        case TokenType::INTEGER_LITERAL: return "INTEGER_LITERAL";
        case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::DOLLAR_STRING: return "DOLLAR_STRING";
        case TokenType::ESCAPE_STRING: return "ESCAPE_STRING";
        case TokenType::BIT_STRING: return "BIT_STRING";
        case TokenType::HEX_STRING: return "HEX_STRING";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::QUOTED_IDENTIFIER: return "QUOTED_IDENTIFIER";
        case TokenType::PARAMETER: return "PARAMETER";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::PERCENT: return "PERCENT";
        case TokenType::CARET: return "CARET";
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::NOT_EQUAL: return "NOT_EQUAL";
        case TokenType::LESS_THAN: return "LESS_THAN";
        case TokenType::GREATER_THAN: return "GREATER_THAN";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::DOUBLE_PIPE: return "DOUBLE_PIPE";
        case TokenType::AMPERSAND: return "AMPERSAND";
        case TokenType::PIPE: return "PIPE";
        case TokenType::TILDE: return "TILDE";
        case TokenType::SHIFT_LEFT: return "SHIFT_LEFT";
        case TokenType::SHIFT_RIGHT: return "SHIFT_RIGHT";
        case TokenType::HASH: return "HASH";
        case TokenType::DOUBLE_COLON: return "DOUBLE_COLON";
        case TokenType::ARROW: return "ARROW";
        case TokenType::DOUBLE_ARROW: return "DOUBLE_ARROW";
        case TokenType::HASH_ARROW: return "HASH_ARROW";
        case TokenType::HASH_DOUBLE_ARROW: return "HASH_DOUBLE_ARROW";
        case TokenType::AT_GREATER: return "AT_GREATER";
        case TokenType::LESS_AT: return "LESS_AT";
        case TokenType::QUESTION: return "QUESTION";
        case TokenType::QUESTION_PIPE: return "QUESTION_PIPE";
        case TokenType::QUESTION_AMPERSAND: return "QUESTION_AMPERSAND";
        case TokenType::AT_QUESTION: return "AT_QUESTION";
        case TokenType::AT_AT: return "AT_AT";
        case TokenType::DOUBLE_AMPERSAND: return "DOUBLE_AMPERSAND";
        case TokenType::LEFT_PAREN: return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN: return "RIGHT_PAREN";
        case TokenType::LEFT_BRACKET: return "LEFT_BRACKET";
        case TokenType::RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TokenType::LEFT_BRACE: return "LEFT_BRACE";
        case TokenType::RIGHT_BRACE: return "RIGHT_BRACE";
        case TokenType::COMMA: return "COMMA";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::DOT: return "DOT";
        case TokenType::COLON: return "COLON";
        case TokenType::KW_SELECT: return "KW_SELECT";
        case TokenType::KW_FROM: return "KW_FROM";
        case TokenType::KW_WHERE: return "KW_WHERE";
        default: return "UNKNOWN";
    }
}

bool isReservedKeyword(TokenType type) {
    return type >= TokenType::KW_SELECT && type < TokenType::TOKEN_TYPE_COUNT;
}

bool isOperator(TokenType type) {
    return type >= TokenType::PLUS && type <= TokenType::AT_SIGN;
}

bool isPunctuation(TokenType type) {
    return type >= TokenType::LEFT_PAREN && type <= TokenType::COLON;
}

bool isTypeKeyword(TokenType type) {
    return type >= TokenType::KW_SMALLINT && type <= TokenType::KW_ZONE;
}

} // namespace scratchbird::parser::postgresql
