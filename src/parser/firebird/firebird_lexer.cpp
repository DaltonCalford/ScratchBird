/**
 * Firebird SQL Lexer Implementation
 *
 * Tokenizes Firebird 5.0 SQL syntax with full reserved keyword recognition.
 */

#include "scratchbird/parser/firebird/firebird_lexer.h"
#include <cctype>
#include <cstring>
#include <algorithm>
#include <charconv>
#include <limits>

namespace scratchbird::parser::firebird {

// ============================================================================
// StringPool Implementation
// ============================================================================

StringPool::StringPool() {
    // Reserve slot 0 for INVALID_ID
    strings_.emplace_back("");
}

StringPool::~StringPool() = default;

StringPool::StringId StringPool::intern(std::string_view str) {
    auto it = lookup_.find(str);
    if (it != lookup_.end()) {
        return it->second;
    }

    StringId id = static_cast<StringId>(strings_.size());
    strings_.emplace_back(str);
    lookup_[std::string_view(strings_.back())] = id;
    return id;
}

std::string_view StringPool::get(StringId id) const {
    if (id == 0 || id >= strings_.size()) {
        return {};
    }
    return strings_[id];
}

void StringPool::clear() {
    strings_.clear();
    lookup_.clear();
    strings_.emplace_back("");  // Reserve slot 0
}

// ============================================================================
// Token Implementation
// ============================================================================

Token::Token() : type(TokenType::ERROR), is_delimited(false) {
    value.int_value = 0;
}

Token Token::makeEOF(SourceLocation loc) {
    Token t;
    t.type = TokenType::END_OF_FILE;
    t.span = SourceSpan(loc, 0);
    t.is_delimited = false;
    return t;
}

Token Token::makeError(SourceLocation loc, uint32_t len) {
    Token t;
    t.type = TokenType::ERROR;
    t.span = SourceSpan(loc, len);
    t.is_delimited = false;
    return t;
}

Token Token::makeInteger(SourceLocation loc, uint32_t len, int64_t val) {
    Token t;
    t.type = TokenType::INTEGER_LITERAL;
    t.span = SourceSpan(loc, len);
    t.is_delimited = false;
    t.value.int_value = val;
    return t;
}

Token Token::makeFloat(SourceLocation loc, uint32_t len, double val) {
    Token t;
    t.type = TokenType::FLOAT_LITERAL;
    t.span = SourceSpan(loc, len);
    t.is_delimited = false;
    t.value.float_value = val;
    return t;
}

Token Token::makeString(SourceLocation loc, uint32_t len, StringPool::StringId id) {
    Token t;
    t.type = TokenType::STRING_LITERAL;
    t.span = SourceSpan(loc, len);
    t.is_delimited = false;
    t.value.string_id = id;
    return t;
}

Token Token::makeQString(SourceLocation loc, uint32_t len, StringPool::StringId id) {
    Token t;
    t.type = TokenType::Q_STRING_LITERAL;
    t.span = SourceSpan(loc, len);
    t.is_delimited = false;
    t.value.string_id = id;
    return t;
}

Token Token::makeBlob(SourceLocation loc, uint32_t len, StringPool::StringId id) {
    Token t;
    t.type = TokenType::BLOB_LITERAL;
    t.span = SourceSpan(loc, len);
    t.is_delimited = false;
    t.value.string_id = id;
    return t;
}

Token Token::makeIdentifier(SourceLocation loc, uint32_t len, StringPool::StringId id, bool delimited) {
    Token t;
    t.type = delimited ? TokenType::QUOTED_IDENTIFIER : TokenType::IDENTIFIER;
    t.span = SourceSpan(loc, len);
    t.is_delimited = delimited;
    t.value.string_id = id;
    return t;
}

Token Token::makeParameter(SourceLocation loc, uint32_t len, StringPool::StringId id) {
    Token t;
    t.type = TokenType::PARAMETER;
    t.span = SourceSpan(loc, len);
    t.is_delimited = false;
    t.value.string_id = id;
    return t;
}

Token Token::makeKeyword(SourceLocation loc, uint32_t len, TokenType kwType) {
    Token t;
    t.type = kwType;
    t.span = SourceSpan(loc, len);
    t.is_delimited = false;
    return t;
}

Token Token::makeOperator(SourceLocation loc, uint32_t len, TokenType opType) {
    Token t;
    t.type = opType;
    t.span = SourceSpan(loc, len);
    t.is_delimited = false;
    return t;
}

Token Token::makePunctuation(SourceLocation loc, uint32_t len, TokenType punctType) {
    Token t;
    t.type = punctType;
    t.span = SourceSpan(loc, len);
    t.is_delimited = false;
    return t;
}

// ============================================================================
// SimpleErrorReporter Implementation
// ============================================================================

void SimpleErrorReporter::reportError(const LexerError& error) {
    errors_.push_back(error);
}

// ============================================================================
// Keyword Tables
// ============================================================================

bool Lexer::tables_initialized_ = false;
std::unordered_map<std::string, TokenType> Lexer::reserved_keywords_;
std::unordered_map<std::string, TokenType> Lexer::non_reserved_keywords_;

void Lexer::initKeywordTables() {
    if (tables_initialized_) return;

    // Reserved keywords (cannot be used as identifiers without quoting)
    reserved_keywords_ = {
        {"ADD", TokenType::KW_ADD},
        {"ADMIN", TokenType::KW_ADMIN},
        {"ALL", TokenType::KW_ALL},
        {"ALTER", TokenType::KW_ALTER},
        {"ALTER_ELEMENT", TokenType::KW_ALTER_ELEMENT},
        {"AND", TokenType::KW_AND},
        {"ANY", TokenType::KW_ANY},
        {"AS", TokenType::KW_AS},
        {"AT", TokenType::KW_AT},
        {"AVG", TokenType::KW_AVG},
        {"BEGIN", TokenType::KW_BEGIN},
        {"BETWEEN", TokenType::KW_BETWEEN},
        {"BIGINT", TokenType::KW_BIGINT},
        {"BINARY", TokenType::KW_BINARY},
        {"BIT_LENGTH", TokenType::KW_BIT_LENGTH},
        {"BLOB", TokenType::KW_BLOB},
        {"BOOLEAN", TokenType::KW_BOOLEAN},
        {"BOTH", TokenType::KW_BOTH},
        {"BY", TokenType::KW_BY},
        {"CASE", TokenType::KW_CASE},
        {"CAST", TokenType::KW_CAST},
        {"CHAR", TokenType::KW_CHAR},
        {"CHARACTER", TokenType::KW_CHARACTER},
        {"CHARACTER_LENGTH", TokenType::KW_CHARACTER_LENGTH},
        {"CHAR_LENGTH", TokenType::KW_CHAR_LENGTH},
        {"CHECK", TokenType::KW_CHECK},
        {"CLOSE", TokenType::KW_CLOSE},
        {"COLLATE", TokenType::KW_COLLATE},
        {"COLUMN", TokenType::KW_COLUMN},
        {"COMMENT", TokenType::KW_COMMENT},
        {"COMMIT", TokenType::KW_COMMIT},
        {"CONNECT", TokenType::KW_CONNECT},
        {"CONSTRAINT", TokenType::KW_CONSTRAINT},
        {"CORR", TokenType::KW_CORR},
        {"COUNT", TokenType::KW_COUNT},
        {"COVAR_POP", TokenType::KW_COVAR_POP},
        {"COVAR_SAMP", TokenType::KW_COVAR_SAMP},
        {"CREATE", TokenType::KW_CREATE},
        {"CROSS", TokenType::KW_CROSS},
        {"CURRENT", TokenType::KW_CURRENT},
        {"CURRENT_CONNECTION", TokenType::KW_CURRENT_CONNECTION},
        {"CURRENT_DATE", TokenType::KW_CURRENT_DATE},
        {"CURRENT_ROLE", TokenType::KW_CURRENT_ROLE},
        {"CURRENT_TIME", TokenType::KW_CURRENT_TIME},
        {"CURRENT_TIMESTAMP", TokenType::KW_CURRENT_TIMESTAMP},
        {"CURRENT_TRANSACTION", TokenType::KW_CURRENT_TRANSACTION},
        {"CURRENT_USER", TokenType::KW_CURRENT_USER},
        {"CURSOR", TokenType::KW_CURSOR},
        {"DATE", TokenType::KW_DATE},
        {"DAY", TokenType::KW_DAY},
        {"DEC", TokenType::KW_DEC},
        {"DECFLOAT", TokenType::KW_DECFLOAT},
        {"DECIMAL", TokenType::KW_DECIMAL},
        {"DECLARE", TokenType::KW_DECLARE},
        {"DEFAULT", TokenType::KW_DEFAULT},
        {"DELETE", TokenType::KW_DELETE},
        {"DELETING", TokenType::KW_DELETING},
        {"DETERMINISTIC", TokenType::KW_DETERMINISTIC},
        {"DISCONNECT", TokenType::KW_DISCONNECT},
        {"DISTINCT", TokenType::KW_DISTINCT},
        {"DOUBLE", TokenType::KW_DOUBLE},
        {"DROP", TokenType::KW_DROP},
        {"ELSE", TokenType::KW_ELSE},
        {"END", TokenType::KW_END},
        {"ESCAPE", TokenType::KW_ESCAPE},
        {"EXECUTE", TokenType::KW_EXECUTE},
        {"EXISTS", TokenType::KW_EXISTS},
        {"EXTERNAL", TokenType::KW_EXTERNAL},
        {"EXTRACT", TokenType::KW_EXTRACT},
        {"FALSE", TokenType::KW_FALSE},
        {"FETCH", TokenType::KW_FETCH},
        {"FILTER", TokenType::KW_FILTER},
        {"FLOAT", TokenType::KW_FLOAT},
        {"FOR", TokenType::KW_FOR},
        {"FOREIGN", TokenType::KW_FOREIGN},
        {"FROM", TokenType::KW_FROM},
        {"FULL", TokenType::KW_FULL},
        {"FUNCTION", TokenType::KW_FUNCTION},
        {"GDSCODE", TokenType::KW_GDSCODE},
        {"GLOBAL", TokenType::KW_GLOBAL},
        {"GRANT", TokenType::KW_GRANT},
        {"GROUP", TokenType::KW_GROUP},
        {"HAVING", TokenType::KW_HAVING},
        {"HOUR", TokenType::KW_HOUR},
        {"IN", TokenType::KW_IN},
        {"INDEX", TokenType::KW_INDEX},
        {"INNER", TokenType::KW_INNER},
        {"INSENSITIVE", TokenType::KW_INSENSITIVE},
        {"INSERT", TokenType::KW_INSERT},
        {"INSERTING", TokenType::KW_INSERTING},
        {"INT", TokenType::KW_INT},
        {"INT128", TokenType::KW_INT128},
        {"UINT128", TokenType::KW_UINT128},
        {"INTEGER", TokenType::KW_INTEGER},
        {"INTO", TokenType::KW_INTO},
        {"IS", TokenType::KW_IS},
        {"JOIN", TokenType::KW_JOIN},
        {"LATERAL", TokenType::KW_LATERAL},
        {"LEADING", TokenType::KW_LEADING},
        {"LEFT", TokenType::KW_LEFT},
        {"LIKE", TokenType::KW_LIKE},
        {"LOCAL", TokenType::KW_LOCAL},
        {"LOCALTIME", TokenType::KW_LOCALTIME},
        {"LOCALTIMESTAMP", TokenType::KW_LOCALTIMESTAMP},
        {"LONG", TokenType::KW_LONG},
        {"LOWER", TokenType::KW_LOWER},
        {"MAX", TokenType::KW_MAX},
        {"MERGE", TokenType::KW_MERGE},
        {"MIN", TokenType::KW_MIN},
        {"MINUTE", TokenType::KW_MINUTE},
        {"MONTH", TokenType::KW_MONTH},
        {"NATIONAL", TokenType::KW_NATIONAL},
        {"NATURAL", TokenType::KW_NATURAL},
        {"NCHAR", TokenType::KW_NCHAR},
        {"NO", TokenType::KW_NO},
        {"NOT", TokenType::KW_NOT},
        {"NULL", TokenType::KW_NULL},
        {"NUMERIC", TokenType::KW_NUMERIC},
        {"OCTET_LENGTH", TokenType::KW_OCTET_LENGTH},
        {"OF", TokenType::KW_OF},
        {"OFFSET", TokenType::KW_OFFSET},
        {"ON", TokenType::KW_ON},
        {"ONLY", TokenType::KW_ONLY},
        {"OPEN", TokenType::KW_OPEN},
        {"OR", TokenType::KW_OR},
        {"ORDER", TokenType::KW_ORDER},
        {"OUTER", TokenType::KW_OUTER},
        {"OVER", TokenType::KW_OVER},
        {"PARAMETER", TokenType::KW_PARAMETER},
        {"PLAN", TokenType::KW_PLAN},
        {"POSITION", TokenType::KW_POSITION},
        {"POST_EVENT", TokenType::KW_POST_EVENT},
        {"PRECISION", TokenType::KW_PRECISION},
        {"PRIMARY", TokenType::KW_PRIMARY},
        {"PROCEDURE", TokenType::KW_PROCEDURE},
        {"PUBLICATION", TokenType::KW_PUBLICATION},
        {"RDB$DB_KEY", TokenType::KW_RDB_DB_KEY},
        {"RDB$ERROR", TokenType::KW_RDB_ERROR},
        {"RDB$GET_CONTEXT", TokenType::KW_RDB_GET_CONTEXT},
        {"RDB$GET_TRANSACTION_CN", TokenType::KW_RDB_GET_TRANSACTION_CN},
        {"RDB$RECORD_VERSION", TokenType::KW_RDB_RECORD_VERSION},
        {"RDB$ROLE_IN_USE", TokenType::KW_RDB_ROLE_IN_USE},
        {"RDB$SET_CONTEXT", TokenType::KW_RDB_SET_CONTEXT},
        {"RDB$SYSTEM_PRIVILEGE", TokenType::KW_RDB_SYSTEM_PRIVILEGE},
        {"REAL", TokenType::KW_REAL},
        {"RECORD_VERSION", TokenType::KW_RECORD_VERSION},
        {"RECREATE", TokenType::KW_RECREATE},
        {"RECURSIVE", TokenType::KW_RECURSIVE},
        {"REFERENCES", TokenType::KW_REFERENCES},
        {"REGR_AVGX", TokenType::KW_REGR_AVGX},
        {"REGR_AVGY", TokenType::KW_REGR_AVGY},
        {"REGR_COUNT", TokenType::KW_REGR_COUNT},
        {"REGR_INTERCEPT", TokenType::KW_REGR_INTERCEPT},
        {"REGR_R2", TokenType::KW_REGR_R2},
        {"REGR_SLOPE", TokenType::KW_REGR_SLOPE},
        {"REGR_SXX", TokenType::KW_REGR_SXX},
        {"REGR_SXY", TokenType::KW_REGR_SXY},
        {"REGR_SYY", TokenType::KW_REGR_SYY},
        {"RELEASE", TokenType::KW_RELEASE},
        {"RESETTING", TokenType::KW_RESETTING},
        {"RETURN", TokenType::KW_RETURN},
        {"RETURNING_VALUES", TokenType::KW_RETURNING_VALUES},
        {"RETURNS", TokenType::KW_RETURNS},
        {"REVOKE", TokenType::KW_REVOKE},
        {"RIGHT", TokenType::KW_RIGHT},
        {"ROLLBACK", TokenType::KW_ROLLBACK},
        {"ROW", TokenType::KW_ROW},
        {"ROWS", TokenType::KW_ROWS},
        {"ROW_COUNT", TokenType::KW_ROW_COUNT},
        {"SAVEPOINT", TokenType::KW_SAVEPOINT},
        {"SCROLL", TokenType::KW_SCROLL},
        {"SECOND", TokenType::KW_SECOND},
        {"SELECT", TokenType::KW_SELECT},
        {"SENSITIVE", TokenType::KW_SENSITIVE},
        {"SET", TokenType::KW_SET},
        {"SIMILAR", TokenType::KW_SIMILAR},
        {"SMALLINT", TokenType::KW_SMALLINT},
        {"SOME", TokenType::KW_SOME},
        {"SQLCODE", TokenType::KW_SQLCODE},
        {"SQLSTATE", TokenType::KW_SQLSTATE},
        {"START", TokenType::KW_START},
        {"STDDEV_POP", TokenType::KW_STDDEV_POP},
        {"STDDEV_SAMP", TokenType::KW_STDDEV_SAMP},
        {"SUM", TokenType::KW_SUM},
        {"TABLE", TokenType::KW_TABLE},
        {"THEN", TokenType::KW_THEN},
        {"TIME", TokenType::KW_TIME},
        {"TIMESTAMP", TokenType::KW_TIMESTAMP},
        {"TIMEZONE_HOUR", TokenType::KW_TIMEZONE_HOUR},
        {"TIMEZONE_MINUTE", TokenType::KW_TIMEZONE_MINUTE},
        {"TO", TokenType::KW_TO},
        {"TRAILING", TokenType::KW_TRAILING},
        {"TRIGGER", TokenType::KW_TRIGGER},
        {"TRIM", TokenType::KW_TRIM},
        {"TRUE", TokenType::KW_TRUE},
        {"UNBOUNDED", TokenType::KW_UNBOUNDED},
        {"UNION", TokenType::KW_UNION},
        {"UNIQUE", TokenType::KW_UNIQUE},
        {"UNKNOWN", TokenType::KW_UNKNOWN},
        {"UPDATE", TokenType::KW_UPDATE},
        {"UPDATING", TokenType::KW_UPDATING},
        {"UPPER", TokenType::KW_UPPER},
        {"USER", TokenType::KW_USER},
        {"USING", TokenType::KW_USING},
        {"VALUE", TokenType::KW_VALUE},
        {"VALUES", TokenType::KW_VALUES},
        {"VARBINARY", TokenType::KW_VARBINARY},
        {"VARCHAR", TokenType::KW_VARCHAR},
        {"VARIABLE", TokenType::KW_VARIABLE},
        {"VARYING", TokenType::KW_VARYING},
        {"VAR_POP", TokenType::KW_VAR_POP},
        {"VAR_SAMP", TokenType::KW_VAR_SAMP},
        {"VIEW", TokenType::KW_VIEW},
        {"WHEN", TokenType::KW_WHEN},
        {"WHERE", TokenType::KW_WHERE},
        {"WHILE", TokenType::KW_WHILE},
        {"WINDOW", TokenType::KW_WINDOW},
        {"WITH", TokenType::KW_WITH},
        {"WITHOUT", TokenType::KW_WITHOUT},
        {"YEAR", TokenType::KW_YEAR},
    };

    // Non-reserved keywords (can be used as identifiers without quoting)
    non_reserved_keywords_ = {
        {"ABS", TokenType::KW_ABS},
        {"ABSOLUTE", TokenType::KW_ABSOLUTE},
        {"ACCENT", TokenType::KW_ACCENT},
        {"ACOS", TokenType::KW_ACOS},
        {"ACOSH", TokenType::KW_ACOSH},
        {"ACTION", TokenType::KW_ACTION},
        {"ACTIVE", TokenType::KW_ACTIVE},
        {"AFTER", TokenType::KW_AFTER},
        {"ALWAYS", TokenType::KW_ALWAYS},
        {"ASC", TokenType::KW_ASC},
        {"ASCENDING", TokenType::KW_ASCENDING},
        {"ASCII_CHAR", TokenType::KW_ASCII_CHAR},
        {"ASCII_VAL", TokenType::KW_ASCII_VAL},
        {"ASIN", TokenType::KW_ASIN},
        {"ASINH", TokenType::KW_ASINH},
        {"ATAN", TokenType::KW_ATAN},
        {"ATAN2", TokenType::KW_ATAN2},
        {"ATANH", TokenType::KW_ATANH},
        {"AUTO", TokenType::KW_AUTO},
        {"AUTONOMOUS", TokenType::KW_AUTONOMOUS},
        {"BACKUP", TokenType::KW_BACKUP},
        {"BASE64_DECODE", TokenType::KW_BASE64_DECODE},
        {"BASE64_ENCODE", TokenType::KW_BASE64_ENCODE},
        {"BEFORE", TokenType::KW_BEFORE},
        {"BIND", TokenType::KW_BIND},
        {"BIN_AND", TokenType::KW_BIN_AND},
        {"BIN_NOT", TokenType::KW_BIN_NOT},
        {"BIN_OR", TokenType::KW_BIN_OR},
        {"BIN_SHL", TokenType::KW_BIN_SHL},
        {"BIN_SHR", TokenType::KW_BIN_SHR},
        {"BIN_XOR", TokenType::KW_BIN_XOR},
        {"BLOB_APPEND", TokenType::KW_BLOB_APPEND},
        {"BLOCK", TokenType::KW_BLOCK},
        {"BODY", TokenType::KW_BODY},
        {"BREAK", TokenType::KW_BREAK},
        {"CALLER", TokenType::KW_CALLER},
        {"CASCADE", TokenType::KW_CASCADE},
        {"CEIL", TokenType::KW_CEIL},
        {"CEILING", TokenType::KW_CEILING},
        {"CHAR_TO_UUID", TokenType::KW_CHAR_TO_UUID},
        {"CLEAR", TokenType::KW_CLEAR},
        {"COALESCE", TokenType::KW_COALESCE},
        {"COLLATION", TokenType::KW_COLLATION},
        {"COMMITTED", TokenType::KW_COMMITTED},
        {"COMMON", TokenType::KW_COMMON},
        {"COMPARE_DECFLOAT", TokenType::KW_COMPARE_DECFLOAT},
        {"COMPUTED", TokenType::KW_COMPUTED},
        {"CONDITIONAL", TokenType::KW_CONDITIONAL},
        {"CONNECTIONS", TokenType::KW_CONNECTIONS},
        {"CONSISTENCY", TokenType::KW_CONSISTENCY},
        {"CONTAINING", TokenType::KW_CONTAINING},
        {"CONTINUE", TokenType::KW_CONTINUE},
        {"COS", TokenType::KW_COS},
        {"COSH", TokenType::KW_COSH},
        {"COT", TokenType::KW_COT},
        {"COUNTER", TokenType::KW_COUNTER},
        {"CRYPT_HASH", TokenType::KW_CRYPT_HASH},
        {"CSTRING", TokenType::KW_CSTRING},
        {"CTR_BIG_ENDIAN", TokenType::KW_CTR_BIG_ENDIAN},
        {"CTR_LENGTH", TokenType::KW_CTR_LENGTH},
        {"CTR_LITTLE_ENDIAN", TokenType::KW_CTR_LITTLE_ENDIAN},
        {"CUME_DIST", TokenType::KW_CUME_DIST},
        {"DATA", TokenType::KW_DATA},
        {"DATABASE", TokenType::KW_DATABASE},
        {"DATEADD", TokenType::KW_DATEADD},
        {"DATEDIFF", TokenType::KW_DATEDIFF},
        {"DDL", TokenType::KW_DDL},
        {"DEBUG", TokenType::KW_DEBUG},
        {"DECODE", TokenType::KW_DECODE},
        {"DECRYPT", TokenType::KW_DECRYPT},
        {"DEFINER", TokenType::KW_DEFINER},
        {"DENSE_RANK", TokenType::KW_DENSE_RANK},
        {"DESC", TokenType::KW_DESC},
        {"DESCENDING", TokenType::KW_DESCENDING},
        {"DESCRIPTOR", TokenType::KW_DESCRIPTOR},
        {"DIFFERENCE", TokenType::KW_DIFFERENCE},
        {"DISABLE", TokenType::KW_DISABLE},
        {"DO", TokenType::KW_DO},
        {"DOMAIN", TokenType::KW_DOMAIN},
        {"ENABLE", TokenType::KW_ENABLE},
        {"ENCRYPT", TokenType::KW_ENCRYPT},
        {"ENGINE", TokenType::KW_ENGINE},
        {"ENTRY_POINT", TokenType::KW_ENTRY_POINT},
        {"EXCEPTION", TokenType::KW_EXCEPTION},
        {"EXCESS", TokenType::KW_EXCESS},
        {"EXCLUDE", TokenType::KW_EXCLUDE},
        {"EXIT", TokenType::KW_EXIT},
        {"EXP", TokenType::KW_EXP},
        {"EXTENDED", TokenType::KW_EXTENDED},
        {"FILE", TokenType::KW_FILE},
        {"FIRST", TokenType::KW_FIRST},
        {"FIRSTNAME", TokenType::KW_FIRSTNAME},
        {"FIRST_DAY", TokenType::KW_FIRST_DAY},
        {"FIRST_VALUE", TokenType::KW_FIRST_VALUE},
        {"FLOOR", TokenType::KW_FLOOR},
        {"FOLLOWING", TokenType::KW_FOLLOWING},
        {"FREE_IT", TokenType::KW_FREE_IT},
        {"GENERATED", TokenType::KW_GENERATED},
        {"GENERATOR", TokenType::KW_GENERATOR},
        {"GEN_ID", TokenType::KW_GEN_ID},
        {"GEN_UUID", TokenType::KW_GEN_UUID},
        {"GRANTED", TokenType::KW_GRANTED},
        {"HASH", TokenType::KW_HASH},
        {"HEX_DECODE", TokenType::KW_HEX_DECODE},
        {"HEX_ENCODE", TokenType::KW_HEX_ENCODE},
        {"IDENTITY", TokenType::KW_IDENTITY},
        {"IDLE", TokenType::KW_IDLE},
        {"IF", TokenType::KW_IF},
        {"IGNORE", TokenType::KW_IGNORE},
        {"IIF", TokenType::KW_IIF},
        {"INACTIVE", TokenType::KW_INACTIVE},
        {"INCLUDE", TokenType::KW_INCLUDE},
        {"INCREMENT", TokenType::KW_INCREMENT},
        {"INPUT_TYPE", TokenType::KW_INPUT_TYPE},
        {"INVOKER", TokenType::KW_INVOKER},
        {"ISOLATION", TokenType::KW_ISOLATION},
        {"IV", TokenType::KW_IV},
        {"KEY", TokenType::KW_KEY},
        {"LAG", TokenType::KW_LAG},
        {"LAST", TokenType::KW_LAST},
        {"LASTNAME", TokenType::KW_LASTNAME},
        {"LAST_DAY", TokenType::KW_LAST_DAY},
        {"LAST_VALUE", TokenType::KW_LAST_VALUE},
        {"LEAD", TokenType::KW_LEAD},
        {"LEAVE", TokenType::KW_LEAVE},
        {"LEGACY", TokenType::KW_LEGACY},
        {"LENGTH", TokenType::KW_LENGTH},
        {"LEVEL", TokenType::KW_LEVEL},
        {"LIFETIME", TokenType::KW_LIFETIME},
        {"LIMBO", TokenType::KW_LIMBO},
        {"LINGER", TokenType::KW_LINGER},
        {"LIST", TokenType::KW_LIST},
        {"LN", TokenType::KW_LN},
        {"LOCK", TokenType::KW_LOCK},
        {"LOCKED", TokenType::KW_LOCKED},
        {"LOG", TokenType::KW_LOG},
        {"LOG10", TokenType::KW_LOG10},
        {"LPAD", TokenType::KW_LPAD},
        {"LPARAM", TokenType::KW_LPARAM},
        {"MAKE_DBKEY", TokenType::KW_MAKE_DBKEY},
        {"MANUAL", TokenType::KW_MANUAL},
        {"MAPPING", TokenType::KW_MAPPING},
        {"MATCHED", TokenType::KW_MATCHED},
        {"MATCHING", TokenType::KW_MATCHING},
        {"MAXVALUE", TokenType::KW_MAXVALUE},
        {"MESSAGE", TokenType::KW_MESSAGE},
        {"MIDDLENAME", TokenType::KW_MIDDLENAME},
        {"MILLISECOND", TokenType::KW_MILLISECOND},
        {"MINVALUE", TokenType::KW_MINVALUE},
        {"MOD", TokenType::KW_MOD},
        {"MODE", TokenType::KW_MODE},
        {"MODULE_NAME", TokenType::KW_MODULE_NAME},
        {"NAME", TokenType::KW_NAME},
        {"NAMES", TokenType::KW_NAMES},
        {"NATIVE", TokenType::KW_NATIVE},
        {"NEXT", TokenType::KW_NEXT},
        {"NORMALIZE_DECFLOAT", TokenType::KW_NORMALIZE_DECFLOAT},
        {"NTH_VALUE", TokenType::KW_NTH_VALUE},
        {"NTILE", TokenType::KW_NTILE},
        {"NULLIF", TokenType::KW_NULLIF},
        {"NULLS", TokenType::KW_NULLS},
        {"NUMBER", TokenType::KW_NUMBER},
        {"OLDEST", TokenType::KW_OLDEST},
        {"OPTION", TokenType::KW_OPTION},
        {"OS_NAME", TokenType::KW_OS_NAME},
        {"OTHERS", TokenType::KW_OTHERS},
        {"OUTPUT_TYPE", TokenType::KW_OUTPUT_TYPE},
        {"OVERFLOW", TokenType::KW_OVERFLOW},
        {"OVERLAY", TokenType::KW_OVERLAY},
        {"OVERRIDING", TokenType::KW_OVERRIDING},
        {"PACKAGE", TokenType::KW_PACKAGE},
        {"PAD", TokenType::KW_PAD},
        {"PAGE", TokenType::KW_PAGE},
        {"PAGES", TokenType::KW_PAGES},
        {"PAGE_SIZE", TokenType::KW_PAGE_SIZE},
        {"PARTITION", TokenType::KW_PARTITION},
        {"PASSWORD", TokenType::KW_PASSWORD},
        {"PERCENT_RANK", TokenType::KW_PERCENT_RANK},
        {"PI", TokenType::KW_PI},
        {"PKCS_1_5", TokenType::KW_PKCS_1_5},
        {"PLACING", TokenType::KW_PLACING},
        {"PLUGIN", TokenType::KW_PLUGIN},
        {"POOL", TokenType::KW_POOL},
        {"POWER", TokenType::KW_POWER},
        {"PRECEDING", TokenType::KW_PRECEDING},
        {"PRESERVE", TokenType::KW_PRESERVE},
        {"PRIOR", TokenType::KW_PRIOR},
        {"PRIVILEGE", TokenType::KW_PRIVILEGE},
        {"PRIVILEGES", TokenType::KW_PRIVILEGES},
        {"PROTECTED", TokenType::KW_PROTECTED},
        {"QUANTIZE", TokenType::KW_QUANTIZE},
        {"RAND", TokenType::KW_RAND},
        {"RANGE", TokenType::KW_RANGE},
        {"RANK", TokenType::KW_RANK},
        {"READ", TokenType::KW_READ},
        {"RELATIVE", TokenType::KW_RELATIVE},
        {"RENAME", TokenType::KW_RENAME},
        {"REPLACE", TokenType::KW_REPLACE},
        {"REQUESTS", TokenType::KW_REQUESTS},
        {"RESERV", TokenType::KW_RESERV},
        {"RESERVING", TokenType::KW_RESERVING},
        {"RESET", TokenType::KW_RESET},
        {"RESTART", TokenType::KW_RESTART},
        {"RESTRICT", TokenType::KW_RESTRICT},
        {"RETAIN", TokenType::KW_RETAIN},
        {"RETURNING", TokenType::KW_RETURNING},
        {"REVERSE", TokenType::KW_REVERSE},
        {"ROLE", TokenType::KW_ROLE},
        {"ROUND", TokenType::KW_ROUND},
        {"ROW_NUMBER", TokenType::KW_ROW_NUMBER},
        {"RPAD", TokenType::KW_RPAD},
        {"RSA_DECRYPT", TokenType::KW_RSA_DECRYPT},
        {"RSA_ENCRYPT", TokenType::KW_RSA_ENCRYPT},
        {"RSA_PRIVATE", TokenType::KW_RSA_PRIVATE},
        {"RSA_PUBLIC", TokenType::KW_RSA_PUBLIC},
        {"RSA_SIGN_HASH", TokenType::KW_RSA_SIGN_HASH},
        {"RSA_VERIFY_HASH", TokenType::KW_RSA_VERIFY_HASH},
        {"SALT_LENGTH", TokenType::KW_SALT_LENGTH},
        {"SCALAR_ARRAY", TokenType::KW_SCALAR_ARRAY},
        {"SCHEMA", TokenType::KW_SCHEMA},
        {"SECURITY", TokenType::KW_SECURITY},
        {"SEGMENT", TokenType::KW_SEGMENT},
        {"SEQUENCE", TokenType::KW_SEQUENCE},
        {"SERVERWIDE", TokenType::KW_SERVERWIDE},
        {"SESSION", TokenType::KW_SESSION},
        {"SHADOW", TokenType::KW_SHADOW},
        {"SHARED", TokenType::KW_SHARED},
        {"SIGN", TokenType::KW_SIGN},
        {"SIGNATURE", TokenType::KW_SIGNATURE},
        {"SIN", TokenType::KW_SIN},
        {"SINGULAR", TokenType::KW_SINGULAR},
        {"SINH", TokenType::KW_SINH},
        {"SIZE", TokenType::KW_SIZE},
        {"SKIP", TokenType::KW_SKIP},
        {"SNAPSHOT", TokenType::KW_SNAPSHOT},
        {"SORT", TokenType::KW_SORT},
        {"SOURCE", TokenType::KW_SOURCE},
        {"SPACE", TokenType::KW_SPACE},
        {"SQL", TokenType::KW_SQL},
        {"SQRT", TokenType::KW_SQRT},
        {"STARTING", TokenType::KW_STARTING},
        {"STATEMENT", TokenType::KW_STATEMENT},
        {"STATISTICS", TokenType::KW_STATISTICS},
        {"SUB_TYPE", TokenType::KW_SUB_TYPE},
        {"SUSPEND", TokenType::KW_SUSPEND},
        {"SYSTEM", TokenType::KW_SYSTEM},
        {"TAGS", TokenType::KW_TAGS},
        {"TAN", TokenType::KW_TAN},
        {"TANH", TokenType::KW_TANH},
        {"TEMPORARY", TokenType::KW_TEMPORARY},
        {"TIES", TokenType::KW_TIES},
        {"TIMEOUT", TokenType::KW_TIMEOUT},
        {"TRANSACTION", TokenType::KW_TRANSACTION},
        {"TRAPS", TokenType::KW_TRAPS},
        {"TRUSTED", TokenType::KW_TRUSTED},
        {"TRUNC", TokenType::KW_TRUNC},
        {"TYPE", TokenType::KW_TYPE},
        {"UNCOMMITTED", TokenType::KW_UNCOMMITTED},
        {"UNDO", TokenType::KW_UNDO},
        {"UNICODE_CHAR", TokenType::KW_UNICODE_CHAR},
        {"UNICODE_VAL", TokenType::KW_UNICODE_VAL},
        {"UUID_TO_CHAR", TokenType::KW_UUID_TO_CHAR},
        {"WAIT", TokenType::KW_WAIT},
        {"WEEK", TokenType::KW_WEEK},
        {"WEEKDAY", TokenType::KW_WEEKDAY},
        {"WORK", TokenType::KW_WORK},
        {"WRITE", TokenType::KW_WRITE},
        {"YEARDAY", TokenType::KW_YEARDAY},
        {"ZONE", TokenType::KW_ZONE},
    };

    tables_initialized_ = true;
}

// ============================================================================
// Lexer Implementation
// ============================================================================

Lexer::Lexer(std::string_view input, SQLDialect dialect)
    : input_(input)
    , pos_(0)
    , line_(1)
    , column_(1)
    , dialect_(dialect)
    , error_reporter_(nullptr) {
    initKeywordTables();
}

Lexer::~Lexer() = default;

char Lexer::current() const {
    return pos_ < input_.size() ? input_[pos_] : '\0';
}

char Lexer::peek(size_t offset) const {
    size_t idx = pos_ + offset;
    return idx < input_.size() ? input_[idx] : '\0';
}

void Lexer::advance() {
    if (pos_ < input_.size()) {
        if (input_[pos_] == '\n') {
            line_++;
            column_ = 1;
        } else {
            column_++;
        }
        pos_++;
    }
}

void Lexer::advanceN(size_t n) {
    for (size_t i = 0; i < n; i++) {
        advance();
    }
}

bool Lexer::atEnd() const {
    return pos_ >= input_.size();
}

SourceLocation Lexer::currentLocation() const {
    return SourceLocation(line_, column_, static_cast<uint32_t>(pos_));
}

std::string_view Lexer::getTokenText(const Token& token) const {
    return getTokenText(token.span);
}

std::string_view Lexer::getTokenText(const SourceSpan& span) const {
    if (span.start.offset + span.length > input_.size()) {
        return {};
    }
    return input_.substr(span.start.offset, span.length);
}

void Lexer::skipWhitespace() {
    while (!atEnd()) {
        char c = current();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '-' && peek() == '-') {
            skipLineComment();
        } else if (c == '/' && peek() == '*') {
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
    while (!atEnd() && current() != '\n') {
        advance();
    }
    if (!atEnd()) {
        advance();  // Skip newline
    }
}

void Lexer::skipBlockComment() {
    // Skip /*
    advance();
    advance();
    while (!atEnd()) {
        if (current() == '*' && peek() == '/') {
            advance();
            advance();
            return;
        }
        advance();
    }
    // Unterminated comment - report error
    reportError(currentLocation(), 0, "Unterminated block comment");
}

Token Lexer::nextToken() {
    if (lookahead_) {
        Token t = *lookahead_;
        lookahead_.reset();
        return t;
    }

    skipWhitespace();

    if (atEnd()) {
        return Token::makeEOF(currentLocation());
    }

    SourceLocation start = currentLocation();
    char c = current();

    // Q-string literals: Q'...' (must check before identifier)
    if ((c == 'Q' || c == 'q') && peek() == '\'') {
        return scanQString();
    }

    // Blob literals: X'...' (must check before identifier)
    if ((c == 'X' || c == 'x') && peek() == '\'') {
        return scanBlobLiteral();
    }

    // Charset-prefixed strings: _charset'...' (must check before identifier)
    if (c == '_' && std::isalpha(peek())) {
        return scanCharsetString();
    }

    // Identifiers and keywords
    if (std::isalpha(c) || c == '_') {
        return scanIdentifierOrKeyword();
    }

    // Numbers
    if (std::isdigit(c)) {
        return scanNumber();
    }

    // String literals
    if (c == '\'') {
        return scanString();
    }

    // Quoted identifiers: "..."
    if (c == '"') {
        return scanQuotedIdentifier();
    }

    // Parameters: :name or ?
    if (c == ':' || c == '?') {
        if (c == ':' && peek() == '=') {
            // := assignment operator
            advance();
            advance();
            return Token::makeOperator(start, 2, TokenType::COLON_EQUALS);
        }
        return scanParameter();
    }

    // Operators and punctuation
    return scanOperator();
}

Token Lexer::peekToken() {
    if (!lookahead_) {
        lookahead_ = nextToken();
    }
    return *lookahead_;
}

Token Lexer::scanIdentifierOrKeyword() {
    SourceLocation start = currentLocation();
    size_t startPos = pos_;

    // Scan identifier characters
    while (!atEnd() && (std::isalnum(current()) || current() == '_' || current() == '$')) {
        advance();
    }

    std::string_view text = input_.substr(startPos, pos_ - startPos);

    // Check for reserved keyword (case-insensitive)
    TokenType kw = lookupKeyword(text);
    if (kw != TokenType::IDENTIFIER) {
        return Token::makeKeyword(start, static_cast<uint32_t>(text.size()), kw);
    }

    // Regular identifier - Firebird uppercases unquoted identifiers
    std::string upper;
    upper.reserve(text.size());
    for (char c : text) {
        upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    StringPool::StringId id = string_pool_.intern(upper);
    return Token::makeIdentifier(start, static_cast<uint32_t>(text.size()), id, false);
}

TokenType Lexer::lookupKeyword(std::string_view text) const {
    // Convert to uppercase for lookup
    std::string upper;
    upper.reserve(text.size());
    for (char c : text) {
        upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }

    // Check reserved keywords first
    auto it = reserved_keywords_.find(upper);
    if (it != reserved_keywords_.end()) {
        return it->second;
    }

    // Also check non-reserved keywords - they still get their keyword type
    // (parser handles context where they can be used as identifiers)
    auto nrit = non_reserved_keywords_.find(upper);
    if (nrit != non_reserved_keywords_.end()) {
        return nrit->second;
    }

    return TokenType::IDENTIFIER;
}

Token Lexer::scanNumber() {
    SourceLocation start = currentLocation();
    size_t startPos = pos_;

    if (current() == '0' && (peek() == 'x' || peek() == 'X')) {
        advance(); // 0
        advance(); // x
        if (!std::isxdigit(static_cast<unsigned char>(current()))) {
            return makeError("Invalid integer number");
        }
        while (!atEnd() && std::isxdigit(static_cast<unsigned char>(current()))) {
            advance();
        }
        std::string_view text = input_.substr(startPos, pos_ - startPos);
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
        return Token::makeInteger(start, static_cast<uint32_t>(text.size()),
                                  static_cast<int64_t>(value));
    }

    // Scan integer part
    while (!atEnd() && std::isdigit(current())) {
        advance();
    }

    // Check for decimal point
    if (current() == '.' && std::isdigit(peek())) {
        advance();  // Skip .
        while (!atEnd() && std::isdigit(current())) {
            advance();
        }

        // Check for exponent
        if (current() == 'e' || current() == 'E') {
            advance();
            if (current() == '+' || current() == '-') {
                advance();
            }
            while (!atEnd() && std::isdigit(current())) {
                advance();
            }
        }

        std::string_view text = input_.substr(startPos, pos_ - startPos);
        double val = 0.0;
        std::from_chars(text.data(), text.data() + text.size(), val);
        return Token::makeFloat(start, static_cast<uint32_t>(text.size()), val);
    }

    // Check for exponent (integer with exponent)
    if (current() == 'e' || current() == 'E') {
        size_t expStart = pos_;
        advance();
        if (current() == '+' || current() == '-') {
            advance();
        }
        if (std::isdigit(current())) {
            while (!atEnd() && std::isdigit(current())) {
                advance();
            }
            std::string_view text = input_.substr(startPos, pos_ - startPos);
            double val = 0.0;
            std::from_chars(text.data(), text.data() + text.size(), val);
            return Token::makeFloat(start, static_cast<uint32_t>(text.size()), val);
        }
        // Not a valid exponent, backtrack
        pos_ = expStart;
    }

    // Integer
    std::string_view text = input_.substr(startPos, pos_ - startPos);
    int64_t val = 0;
    std::from_chars(text.data(), text.data() + text.size(), val);
    return Token::makeInteger(start, static_cast<uint32_t>(text.size()), val);
}

Token Lexer::scanString() {
    SourceLocation start = currentLocation();
    size_t startPos = pos_;
    std::string value;
    bool terminated = false;

    advance();  // Skip opening quote

    while (!atEnd()) {
        char c = current();
        if (c == '\'') {
            if (peek() == '\'') {
                // Escaped quote
                value.push_back('\'');
                advance();
                advance();
            } else {
                // End of string
                advance();
                terminated = true;
                break;
            }
        } else {
            value.push_back(c);
            advance();
        }
    }

    if (!terminated) {
        reportError(start, static_cast<uint32_t>(pos_ - startPos),
                    "Unterminated string literal", "Add closing quote");
        return makeError("Unterminated string literal");
    }

    StringPool::StringId id = string_pool_.intern(value);
    return Token::makeString(start, static_cast<uint32_t>(pos_ - startPos), id);
}

Token Lexer::scanQString() {
    SourceLocation start = currentLocation();
    size_t startPos = pos_;

    advance();  // Skip Q
    advance();  // Skip opening quote

    if (atEnd()) {
        return makeError("Unexpected end of input in Q-string");
    }

    char delimiter = current();
    char closeDelim;
    switch (delimiter) {
        case '{': closeDelim = '}'; break;
        case '[': closeDelim = ']'; break;
        case '(': closeDelim = ')'; break;
        case '<': closeDelim = '>'; break;
        default:
            // Any other character is both open and close
            closeDelim = delimiter;
            break;
    }
    advance();  // Skip opening delimiter

    std::string value;
    while (!atEnd()) {
        char c = current();
        if (c == closeDelim && peek() == '\'') {
            advance();  // Skip close delimiter
            advance();  // Skip closing quote
            break;
        }
        value.push_back(c);
        advance();
    }

    StringPool::StringId id = string_pool_.intern(value);
    return Token::makeQString(start, static_cast<uint32_t>(pos_ - startPos), id);
}

Token Lexer::scanCharsetString() {
    SourceLocation start = currentLocation();
    size_t startPos = pos_;

    advance();  // Skip _

    // Scan charset name
    while (!atEnd() && (std::isalnum(current()) || current() == '_')) {
        advance();
    }

    if (current() != '\'') {
        // Not a charset string, backtrack and treat as identifier
        pos_ = startPos;
        line_ = start.line;
        column_ = start.column;
        return scanIdentifierOrKeyword();
    }

    // Now scan the string part
    std::string value;
    advance();  // Skip quote

    while (!atEnd()) {
        char c = current();
        if (c == '\'') {
            if (peek() == '\'') {
                value.push_back('\'');
                advance();
                advance();
            } else {
                advance();
                break;
            }
        } else {
            value.push_back(c);
            advance();
        }
    }

    StringPool::StringId id = string_pool_.intern(value);
    return Token::makeString(start, static_cast<uint32_t>(pos_ - startPos), id);
}

Token Lexer::scanBlobLiteral() {
    SourceLocation start = currentLocation();
    size_t startPos = pos_;

    advance();  // Skip X
    advance();  // Skip quote

    std::string value;
    while (!atEnd()) {
        char c = current();
        if (c == '\'') {
            advance();
            break;
        }
        if (std::isxdigit(c)) {
            value.push_back(c);
            advance();
        } else if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            // Skip whitespace in blob literals
            advance();
        } else {
            return makeError("Invalid character in blob literal");
        }
    }

    StringPool::StringId id = string_pool_.intern(value);
    return Token::makeBlob(start, static_cast<uint32_t>(pos_ - startPos), id);
}

Token Lexer::scanQuotedIdentifier() {
    SourceLocation start = currentLocation();
    size_t startPos = pos_;

    advance();  // Skip opening quote

    std::string value;
    while (!atEnd()) {
        char c = current();
        if (c == '"') {
            if (peek() == '"') {
                // Escaped quote
                value.push_back('"');
                advance();
                advance();
            } else {
                // End of identifier
                advance();
                break;
            }
        } else {
            value.push_back(c);
            advance();
        }
    }

    StringPool::StringId id = string_pool_.intern(value);
    return Token::makeIdentifier(start, static_cast<uint32_t>(pos_ - startPos), id, true);
}

Token Lexer::scanParameter() {
    SourceLocation start = currentLocation();
    char c = current();

    if (c == '?') {
        advance();
        // Positional parameter ?
        StringPool::StringId id = string_pool_.intern("?");
        return Token::makeParameter(start, 1, id);
    }

    // Named parameter :name
    advance();  // Skip :

    size_t startPos = pos_;
    while (!atEnd() && (std::isalnum(current()) || current() == '_')) {
        advance();
    }

    std::string_view name = input_.substr(startPos, pos_ - startPos);
    if (name.empty()) {
        return makeError("Expected parameter name after :");
    }

    StringPool::StringId id = string_pool_.intern(name);
    return Token::makeParameter(start, static_cast<uint32_t>(1 + name.size()), id);
}

Token Lexer::scanOperator() {
    SourceLocation start = currentLocation();
    char c = current();

    switch (c) {
        case '+':
            advance();
            return Token::makeOperator(start, 1, TokenType::PLUS);
        case '-':
            advance();
            return Token::makeOperator(start, 1, TokenType::MINUS);
        case '*':
            advance();
            return Token::makeOperator(start, 1, TokenType::STAR);
        case '/':
            advance();
            return Token::makeOperator(start, 1, TokenType::SLASH);
        case '(':
            advance();
            return Token::makePunctuation(start, 1, TokenType::LEFT_PAREN);
        case ')':
            advance();
            return Token::makePunctuation(start, 1, TokenType::RIGHT_PAREN);
        case '[':
            advance();
            return Token::makePunctuation(start, 1, TokenType::LEFT_BRACKET);
        case ']':
            advance();
            return Token::makePunctuation(start, 1, TokenType::RIGHT_BRACKET);
        case ',':
            advance();
            return Token::makePunctuation(start, 1, TokenType::COMMA);
        case ';':
            advance();
            return Token::makePunctuation(start, 1, TokenType::SEMICOLON);
        case '.':
            advance();
            return Token::makePunctuation(start, 1, TokenType::DOT);
        case ':':
            advance();
            return Token::makePunctuation(start, 1, TokenType::COLON);
        case '=':
            advance();
            return Token::makeOperator(start, 1, TokenType::EQUAL);
        case '<':
            advance();
            if (current() == '=') {
                advance();
                return Token::makeOperator(start, 2, TokenType::LESS_EQUAL);
            } else if (current() == '>') {
                advance();
                return Token::makeOperator(start, 2, TokenType::NOT_EQUAL);
            }
            return Token::makeOperator(start, 1, TokenType::LESS_THAN);
        case '>':
            advance();
            if (current() == '=') {
                advance();
                return Token::makeOperator(start, 2, TokenType::GREATER_EQUAL);
            }
            return Token::makeOperator(start, 1, TokenType::GREATER_THAN);
        case '|':
            advance();
            if (current() == '|') {
                advance();
                return Token::makeOperator(start, 2, TokenType::DOUBLE_PIPE);
            }
            return makeError("Expected || for concatenation");
        case '!':
            advance();
            if (current() == '=') {
                advance();
                return Token::makeOperator(start, 2, TokenType::NOT_EQUAL_BANG);
            } else if (current() == '<') {
                advance();
                return Token::makeOperator(start, 2, TokenType::NOT_LESS);
            } else if (current() == '>') {
                advance();
                return Token::makeOperator(start, 2, TokenType::NOT_GREATER);
            }
            return makeError("Unexpected character after !");
        case '~':
            advance();
            if (current() == '=') {
                advance();
                return Token::makeOperator(start, 2, TokenType::NOT_EQUAL_TILDE);
            } else if (current() == '<') {
                advance();
                return Token::makeOperator(start, 2, TokenType::NOT_LESS);
            } else if (current() == '>') {
                advance();
                return Token::makeOperator(start, 2, TokenType::NOT_GREATER);
            }
            return makeError("Unexpected character after ~");
        case '^':
            advance();
            if (current() == '=') {
                advance();
                return Token::makeOperator(start, 2, TokenType::NOT_EQUAL_CARET);
            } else if (current() == '<') {
                advance();
                return Token::makeOperator(start, 2, TokenType::NOT_LESS);
            } else if (current() == '>') {
                advance();
                return Token::makeOperator(start, 2, TokenType::NOT_GREATER);
            }
            return makeError("Unexpected character after ^");
        default:
            advance();
            return makeError("Unexpected character");
    }
}

Token Lexer::makeError(const std::string& message) {
    SourceLocation loc = currentLocation();
    reportError(loc, 1, message);
    return Token::makeError(loc, 1);
}

void Lexer::reportError(SourceLocation loc, uint32_t len, const std::string& message,
                         const std::string& hint) {
    if (error_reporter_) {
        LexerError error;
        error.span = SourceSpan(loc, len);
        error.message = message;
        error.hint = hint;
        error_reporter_->reportError(error);
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::END_OF_FILE: return "END_OF_FILE";
        case TokenType::ERROR: return "ERROR";
        case TokenType::INTEGER_LITERAL: return "INTEGER_LITERAL";
        case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::Q_STRING_LITERAL: return "Q_STRING_LITERAL";
        case TokenType::BLOB_LITERAL: return "BLOB_LITERAL";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::QUOTED_IDENTIFIER: return "QUOTED_IDENTIFIER";
        case TokenType::PARAMETER: return "PARAMETER";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::DOUBLE_PIPE: return "DOUBLE_PIPE";
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::NOT_EQUAL: return "NOT_EQUAL";
        case TokenType::NOT_EQUAL_BANG: return "NOT_EQUAL_BANG";
        case TokenType::LESS_THAN: return "LESS_THAN";
        case TokenType::GREATER_THAN: return "GREATER_THAN";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::NOT_LESS: return "NOT_LESS";
        case TokenType::NOT_GREATER: return "NOT_GREATER";
        case TokenType::NOT_EQUAL_TILDE: return "NOT_EQUAL_TILDE";
        case TokenType::NOT_EQUAL_CARET: return "NOT_EQUAL_CARET";
        case TokenType::COLON_EQUALS: return "COLON_EQUALS";
        case TokenType::LEFT_PAREN: return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN: return "RIGHT_PAREN";
        case TokenType::LEFT_BRACKET: return "LEFT_BRACKET";
        case TokenType::RIGHT_BRACKET: return "RIGHT_BRACKET";
        case TokenType::COMMA: return "COMMA";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::DOT: return "DOT";
        case TokenType::COLON: return "COLON";
        case TokenType::KW_SELECT: return "SELECT";
        case TokenType::KW_FROM: return "FROM";
        case TokenType::KW_WHERE: return "WHERE";
        case TokenType::KW_INSERT: return "INSERT";
        case TokenType::KW_UPDATE: return "UPDATE";
        case TokenType::KW_DELETE: return "DELETE";
        case TokenType::KW_CREATE: return "CREATE";
        case TokenType::KW_ALTER: return "ALTER";
        case TokenType::KW_ALTER_ELEMENT: return "ALTER_ELEMENT";
        case TokenType::KW_DROP: return "DROP";
        case TokenType::KW_TABLE: return "TABLE";
        case TokenType::KW_INDEX: return "INDEX";
        case TokenType::KW_VIEW: return "VIEW";
        case TokenType::KW_PROCEDURE: return "PROCEDURE";
        case TokenType::KW_FUNCTION: return "FUNCTION";
        case TokenType::KW_TRIGGER: return "TRIGGER";
        case TokenType::KW_BEGIN: return "BEGIN";
        case TokenType::KW_END: return "END";
        case TokenType::KW_IF: return "IF";
        case TokenType::KW_THEN: return "THEN";
        case TokenType::KW_ELSE: return "ELSE";
        case TokenType::KW_WHILE: return "WHILE";
        case TokenType::KW_FOR: return "FOR";
        case TokenType::KW_RETURN: return "RETURN";
        case TokenType::KW_AND: return "AND";
        case TokenType::KW_OR: return "OR";
        case TokenType::KW_NOT: return "NOT";
        case TokenType::KW_NULL: return "NULL";
        case TokenType::KW_TRUE: return "TRUE";
        case TokenType::KW_FALSE: return "FALSE";
        default: return "UNKNOWN";
    }
}

bool isReservedKeyword(TokenType type) {
    return type >= TokenType::KW_ADD && type <= TokenType::KW_YEAR;
}

bool isNonReservedKeyword(TokenType type) {
    return type >= TokenType::KW_ABS && type <= TokenType::KW_ZONE;
}

bool isOperator(TokenType type) {
    return type >= TokenType::PLUS && type <= TokenType::COLON_EQUALS;
}

bool isPunctuation(TokenType type) {
    return type >= TokenType::LEFT_PAREN && type <= TokenType::COLON;
}

} // namespace scratchbird::parser::firebird
