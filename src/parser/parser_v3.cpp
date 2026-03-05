/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
/**
 * ScratchBird Parser v3.0 - Main Parser Implementation
 *
 * See: include/scratchbird/parser/parser_v3.h
 */

#include "scratchbird/parser/parser_v3.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/sblr/extract_element_catalog.h"
#include <cctype>
#include <cstring>
#include <algorithm>
#include <set>
#include <utility>

namespace {

struct ParsedTimeTz {
    int64_t time_usec = 0;
    int16_t offset_minutes = 0;
    std::string tz_name;
};

struct ParsedTimestampTz {
    int64_t epoch_usec = 0;
    int16_t offset_minutes = 0;
    std::string tz_name;
};

static int64_t daysFromCivil(int y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<int64_t>(era) * 146097 + static_cast<int64_t>(doe) - 719468;
}

static bool parseTwoDigits(std::string_view s, size_t& pos, int& out) {
    if (pos + 2 > s.size()) return false;
    if (!std::isdigit(static_cast<unsigned char>(s[pos])) ||
        !std::isdigit(static_cast<unsigned char>(s[pos + 1]))) {
        return false;
    }
    out = (s[pos] - '0') * 10 + (s[pos + 1] - '0');
    pos += 2;
    return true;
}

static bool parseIntN(std::string_view s, size_t& pos, int n, int& out) {
    if (pos + static_cast<size_t>(n) > s.size()) return false;
    int value = 0;
    for (int i = 0; i < n; ++i) {
        char c = s[pos + static_cast<size_t>(i)];
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
        value = value * 10 + (c - '0');
    }
    pos += static_cast<size_t>(n);
    out = value;
    return true;
}

static void parseFraction(std::string_view s, size_t& pos, int64_t& usec_out) {
    usec_out = 0;
    if (pos >= s.size() || s[pos] != '.') return;
    ++pos;
    int digits = 0;
    while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos])) && digits < 6) {
        usec_out = usec_out * 10 + (s[pos] - '0');
        ++pos;
        ++digits;
    }
    while (digits++ < 6) {
        usec_out *= 10;
    }
    while (pos < s.size() && std::isdigit(static_cast<unsigned char>(s[pos]))) {
        ++pos;
    }
}

static bool parseTimeCore(std::string_view s, size_t& pos, int64_t& time_usec_out) {
    int hh = 0;
    int mm = 0;
    int ss = 0;
    if (!parseTwoDigits(s, pos, hh)) return false;
    if (pos >= s.size() || s[pos] != ':') return false;
    ++pos;
    if (!parseTwoDigits(s, pos, mm)) return false;
    if (pos < s.size() && s[pos] == ':') {
        ++pos;
        if (!parseTwoDigits(s, pos, ss)) return false;
    }
    int64_t frac = 0;
    parseFraction(s, pos, frac);
    time_usec_out = (static_cast<int64_t>(hh) * 3600 +
                     static_cast<int64_t>(mm) * 60 +
                     static_cast<int64_t>(ss)) * 1000000 + frac;
    return true;
}

static bool parseTimeTz(std::string_view s, ParsedTimeTz& out) {
    auto trim = [](std::string_view v) {
        size_t b = 0;
        while (b < v.size() && std::isspace(static_cast<unsigned char>(v[b]))) ++b;
        size_t e = v.size();
        while (e > b && std::isspace(static_cast<unsigned char>(v[e - 1]))) --e;
        return v.substr(b, e - b);
    };
    s = trim(s);
    size_t pos = 0;
    if (!parseTimeCore(s, pos, out.time_usec)) return false;

    if (pos < s.size() && std::isspace(static_cast<unsigned char>(s[pos]))) {
        ++pos;
        out.tz_name = std::string(trim(s.substr(pos)));
        return true;
    }

    if (pos < s.size()) {
        char c = s[pos];
        if (c == 'Z') {
            out.offset_minutes = 0;
            return true;
        }
        if (c == '+' || c == '-') {
            int sign = (c == '-') ? -1 : 1;
            ++pos;
            int oh = 0;
            int om = 0;
            if (!parseTwoDigits(s, pos, oh)) return false;
            if (pos < s.size() && s[pos] == ':') {
                ++pos;
                if (!parseTwoDigits(s, pos, om)) return false;
            }
            out.offset_minutes = static_cast<int16_t>(sign * (oh * 60 + om));
            return true;
        }
    }
    return true;
}

static bool parseTimestampTz(std::string_view s, ParsedTimestampTz& out) {
    auto trim = [](std::string_view v) {
        size_t b = 0;
        while (b < v.size() && std::isspace(static_cast<unsigned char>(v[b]))) ++b;
        size_t e = v.size();
        while (e > b && std::isspace(static_cast<unsigned char>(v[e - 1]))) --e;
        return v.substr(b, e - b);
    };
    s = trim(s);
    size_t pos = 0;
    int year = 0, mon = 0, day = 0;
    if (!parseIntN(s, pos, 4, year)) return false;
    if (pos >= s.size() || s[pos] != '-') return false;
    ++pos;
    if (!parseTwoDigits(s, pos, mon)) return false;
    if (pos >= s.size() || s[pos] != '-') return false;
    ++pos;
    if (!parseTwoDigits(s, pos, day)) return false;

    if (pos < s.size() && (s[pos] == 'T' || s[pos] == ' ')) ++pos;
    int64_t time_usec = 0;
    if (!parseTimeCore(s, pos, time_usec)) return false;

    ParsedTimeTz tz;
    if (pos < s.size()) {
        if (std::isspace(static_cast<unsigned char>(s[pos]))) {
            ++pos;
            tz.tz_name = std::string(trim(s.substr(pos)));
        } else {
            std::string_view rest = s.substr(pos);
            parseTimeTz(rest, tz);
        }
    }

    int64_t days = daysFromCivil(year, static_cast<unsigned>(mon), static_cast<unsigned>(day));
    int64_t epoch_usec = (days * 86400LL * 1000000LL) + time_usec;
    epoch_usec -= static_cast<int64_t>(tz.offset_minutes) * 60LL * 1000000LL;
    out.epoch_usec = epoch_usec;
    out.offset_minutes = tz.offset_minutes;
    out.tz_name = tz.tz_name;
    return true;
}

static bool parseUuidBytes(std::string_view s, scratchbird::parser::v3::U128& out) {
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    std::string hexs;
    hexs.reserve(32);
    for (char c : s) {
        if (c == '-') continue;
        if (hex(c) < 0) return false;
        hexs.push_back(c);
    }
    if (hexs.size() != 32) return false;
    for (size_t i = 0; i < 16; ++i) {
        int hi = hex(hexs[i * 2]);
        int lo = hex(hexs[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

static bool parseUnsigned128(std::string_view s, scratchbird::core::uint128_t& out) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    if (s.empty()) return false;

    int base = 10;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        s.remove_prefix(2);
    }
    if (s.empty()) return false;

    scratchbird::core::uint128_t value = 0;
    const scratchbird::core::uint128_t maxv = ~static_cast<scratchbird::core::uint128_t>(0);
    for (char c : s) {
        int digit = -1;
        if (base == 10) {
            if (c >= '0' && c <= '9') digit = c - '0';
        } else {
            if (c >= '0' && c <= '9') digit = c - '0';
            else if (c >= 'a' && c <= 'f') digit = 10 + (c - 'a');
            else if (c >= 'A' && c <= 'F') digit = 10 + (c - 'A');
        }
        if (digit < 0 || digit >= base) return false;
        if (value > (maxv - static_cast<scratchbird::core::uint128_t>(digit)) /
                        static_cast<scratchbird::core::uint128_t>(base)) {
            return false;
        }
        value = value * static_cast<scratchbird::core::uint128_t>(base) +
                static_cast<scratchbird::core::uint128_t>(digit);
    }
    out = value;
    return true;
}

static bool parseSigned128(std::string_view s, scratchbird::core::uint128_t& out) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    if (s.empty()) return false;
    bool neg = false;
    if (s.front() == '+' || s.front() == '-') {
        neg = (s.front() == '-');
        s.remove_prefix(1);
    }
    scratchbird::core::uint128_t mag = 0;
    if (!parseUnsigned128(s, mag)) return false;
    const scratchbird::core::uint128_t max_pos =
        (static_cast<scratchbird::core::uint128_t>(1) << 127) - 1;
    const scratchbird::core::uint128_t max_neg =
        (static_cast<scratchbird::core::uint128_t>(1) << 127);
    if (neg) {
        if (mag > max_neg) return false;
        scratchbird::core::uint128_t val = (~mag) + 1;
        out = val;
    } else {
        if (mag > max_pos) return false;
        out = mag;
    }
    return true;
}

static void storeU128LE(scratchbird::core::uint128_t value, scratchbird::parser::v3::U128& out) {
    for (size_t i = 0; i < out.size(); ++i) {
        out[i] = static_cast<uint8_t>(value & 0xFF);
        value >>= 8;
    }
}

} // namespace
#include <limits>

namespace scratchbird::parser::v3 {

namespace {

constexpr char kFeatureDocPathFilter[] = "F_DOC_PATH_FILTER";
constexpr char kFeatureTsBucketAgg[] = "F_TS_BUCKET_AGG";
constexpr char kFeatureScheduleRruleSurface[] = "F_RRULE_SCHEDULE_SURFACE";
constexpr char kFeatureSearchQueryDsl[] = "F_SEARCH_QUERY_DSL";
constexpr char kFeatureVectorAnn[] = "F_VECTOR_ANN";
constexpr char kFeatureHybridBridgeHint[] = "F_HYBRID_BRIDGE_HINT";
constexpr char kFeatureEngineProfileCreate[] = "F_ENGINE_PROFILE_CREATE";
constexpr char kFeatureSecurityUserAccountDdl[] = "F_SECURITY_USER_ACCOUNT_DDL";
constexpr char kFeatureSecurityConnectionRuleDdl[] = "F_SECURITY_CONNECTION_RULE_DDL";
constexpr char kFeatureSecurityTokenDdl[] = "F_SECURITY_TOKEN_DDL";
constexpr char kFeatureSecurityQuotaProfileDdl[] = "F_SECURITY_QUOTA_PROFILE_DDL";
constexpr char kFeatureSecurityModelPolicyDdl[] = "F_SECURITY_MODEL_POLICY_DDL";
constexpr char kFeatureLanguageUdrCompileBridge[] = "F_LANGUAGE_UDR_COMPILE_BRIDGE";
constexpr char kFeatureEmbeddedSqlTemplateCompile[] = "F_EMBEDDED_SQL_TEMPLATE_COMPILE";
constexpr char kFeatureDmlMysqlOnDuplicateKey[] = "F_DML_MYSQL_ON_DUPLICATE_KEY";
constexpr char kFeatureDmlUpdateOrderLimit[] = "F_DML_UPDATE_ORDER_LIMIT";
constexpr char kFeatureDmlDeleteOrderLimit[] = "F_DML_DELETE_ORDER_LIMIT";
constexpr char kFeatureDmlWritableCte[] = "F_DML_WRITABLE_CTE";
constexpr char kFeatureDmlMergeNotMatchedBySource[] = "F_DML_MERGE_NOT_MATCHED_BY_SOURCE";

std::string toUpperAscii(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    }
    return value;
}

std::set<std::string> normalizeFeatureKeySet(const std::set<std::string>& input) {
    std::set<std::string> normalized;
    for (const std::string& value : input) {
        normalized.insert(toUpperAscii(value));
    }
    return normalized;
}

} // namespace

static std::optional<IndexType> indexTypeFromName(std::string_view name) {
    if (caseInsensitiveEquals(name, "BTREE")) return IndexType::BTREE;
    if (caseInsensitiveEquals(name, "HASH")) return IndexType::HASH;
    if (caseInsensitiveEquals(name, "HNSW")) return IndexType::HNSW;
    if (caseInsensitiveEquals(name, "FULLTEXT")) return IndexType::FULLTEXT;
    if (caseInsensitiveEquals(name, "GIN")) return IndexType::GIN;
    if (caseInsensitiveEquals(name, "GIST")) return IndexType::GIST;
    if (caseInsensitiveEquals(name, "BRIN")) return IndexType::BRIN;
    if (caseInsensitiveEquals(name, "RTREE")) return IndexType::RTREE;
    if (caseInsensitiveEquals(name, "SPGIST")) return IndexType::SPGIST;
    if (caseInsensitiveEquals(name, "BITMAP")) return IndexType::BITMAP;
    if (caseInsensitiveEquals(name, "COLUMNSTORE")) return IndexType::COLUMNSTORE;
    if (caseInsensitiveEquals(name, "LSM")) return IndexType::LSM;
    if (caseInsensitiveEquals(name, "IVF")) return IndexType::IVF;
    if (caseInsensitiveEquals(name, "ZONEMAP")) return IndexType::ZONEMAP;
    if (caseInsensitiveEquals(name, "ART")) return IndexType::ART;
    if (caseInsensitiveEquals(name, "BLOOM")) return IndexType::BLOOM;
    if (caseInsensitiveEquals(name, "VECTOR_FLAT")) return IndexType::VECTOR_FLAT;
    if (caseInsensitiveEquals(name, "VECTOR_BIN_FLAT")) return IndexType::VECTOR_BIN_FLAT;
    if (caseInsensitiveEquals(name, "IVF_FLAT")) return IndexType::IVF_FLAT;
    if (caseInsensitiveEquals(name, "BIN_IVF_FLAT")) return IndexType::BIN_IVF_FLAT;
    if (caseInsensitiveEquals(name, "IVF_PQ")) return IndexType::IVF_PQ;
    if (caseInsensitiveEquals(name, "IVF_SQ8")) return IndexType::IVF_SQ8;
    if (caseInsensitiveEquals(name, "IVF_SQ8_HYBRID")) return IndexType::IVF_SQ8_HYBRID;
    if (caseInsensitiveEquals(name, "RHNSW_PQ")) return IndexType::RHNSW_PQ;
    if (caseInsensitiveEquals(name, "RHNSW_SQ")) return IndexType::RHNSW_SQ;
    if (caseInsensitiveEquals(name, "ANNOY")) return IndexType::ANNOY;
    if (caseInsensitiveEquals(name, "NSG")) return IndexType::NSG;
    if (caseInsensitiveEquals(name, "DISKANN")) return IndexType::DISKANN;
    if (caseInsensitiveEquals(name, "SCANN")) return IndexType::SCANN;
    if (caseInsensitiveEquals(name, "GPU_CAGRA")) return IndexType::GPU_CAGRA;
    if (caseInsensitiveEquals(name, "MINHASH_LSH")) return IndexType::MINHASH_LSH;
    if (caseInsensitiveEquals(name, "SPARSE_INVERTED")) return IndexType::SPARSE_INVERTED;
    if (caseInsensitiveEquals(name, "SPARSE_WAND")) return IndexType::SPARSE_WAND;
    if (caseInsensitiveEquals(name, "TRIE")) return IndexType::TRIE;
    if (caseInsensitiveEquals(name, "INVERTED")) return IndexType::INVERTED;
    if (caseInsensitiveEquals(name, "STL_SORT")) return IndexType::STL_SORT;
    if (caseInsensitiveEquals(name, "NGRAM")) return IndexType::NGRAM;
    if (caseInsensitiveEquals(name, "MONGODB_2D")) return IndexType::MONGODB_2D;
    if (caseInsensitiveEquals(name, "MONGODB_2DSPHERE")) return IndexType::MONGODB_2DSPHERE;
    if (caseInsensitiveEquals(name, "MONGODB_2DSPHERE_BUCKET")) return IndexType::MONGODB_2DSPHERE_BUCKET;
    if (caseInsensitiveEquals(name, "MONGODB_GEO_HAYSTACK")) return IndexType::MONGODB_GEO_HAYSTACK;
    if (caseInsensitiveEquals(name, "MONGODB_WILDCARD")) return IndexType::MONGODB_WILDCARD;
    if (caseInsensitiveEquals(name, "MONGODB_ENCRYPTED_RANGE")) return IndexType::MONGODB_ENCRYPTED_RANGE;
    if (caseInsensitiveEquals(name, "NEO4J_LOOKUP")) return IndexType::NEO4J_LOOKUP;
    if (caseInsensitiveEquals(name, "NEO4J_TEXT")) return IndexType::NEO4J_TEXT;
    if (caseInsensitiveEquals(name, "NEO4J_RANGE")) return IndexType::NEO4J_RANGE;
    if (caseInsensitiveEquals(name, "NEO4J_POINT")) return IndexType::NEO4J_POINT;
    if (caseInsensitiveEquals(name, "NEO4J_VECTOR")) return IndexType::NEO4J_VECTOR;
    if (caseInsensitiveEquals(name, "CASSANDRA_SASI")) return IndexType::CASSANDRA_SASI;
    if (caseInsensitiveEquals(name, "CASSANDRA_SAI")) return IndexType::CASSANDRA_SAI;
    if (caseInsensitiveEquals(name, "REDIS_STRING")) return IndexType::REDIS_STRING;
    if (caseInsensitiveEquals(name, "REDIS_HASH")) return IndexType::REDIS_HASH;
    if (caseInsensitiveEquals(name, "REDIS_LIST")) return IndexType::REDIS_LIST;
    if (caseInsensitiveEquals(name, "REDIS_SET")) return IndexType::REDIS_SET;
    if (caseInsensitiveEquals(name, "REDIS_ZSET")) return IndexType::REDIS_ZSET;
    if (caseInsensitiveEquals(name, "REDIS_STREAM")) return IndexType::REDIS_STREAM;
    if (caseInsensitiveEquals(name, "REDIS_BITMAP")) return IndexType::REDIS_BITMAP;
    if (caseInsensitiveEquals(name, "REDIS_HLL")) return IndexType::REDIS_HLL;
    if (caseInsensitiveEquals(name, "REDIS_GEO")) return IndexType::REDIS_GEO;
    return std::nullopt;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

Parser::Parser(std::string_view input)
    : Parser(input, ParserOptions{})
{
}

Parser::Parser(std::string_view input, ParserOptions options)
    : lexer_(input)
    , state_(lexer_)
    , arena_()
    , options_(std::move(options))
{
    initializeCapabilityProfile();
}

Parser::~Parser() = default;

std::set<std::string> Parser::defaultCapabilitySetForProfile(std::string_view profile) {
    const std::string normalized_profile = toUpperAscii(std::string(profile));
    if (normalized_profile == "NATIVE" || normalized_profile.empty()) {
        return {
            kFeatureDocPathFilter,
            kFeatureTsBucketAgg,
            kFeatureScheduleRruleSurface,
            kFeatureSearchQueryDsl,
            kFeatureVectorAnn,
            kFeatureHybridBridgeHint,
            kFeatureEngineProfileCreate,
            kFeatureSecurityUserAccountDdl,
            kFeatureSecurityConnectionRuleDdl,
            kFeatureSecurityTokenDdl,
            kFeatureSecurityQuotaProfileDdl,
            kFeatureSecurityModelPolicyDdl,
            kFeatureLanguageUdrCompileBridge,
            kFeatureEmbeddedSqlTemplateCompile,
            kFeatureDmlMysqlOnDuplicateKey,
            kFeatureDmlUpdateOrderLimit,
            kFeatureDmlDeleteOrderLimit,
            kFeatureDmlWritableCte,
            kFeatureDmlMergeNotMatchedBySource
        };
    }
    return {};
}

void Parser::initializeCapabilityProfile() {
    if (options_.active_profile.empty()) {
        options_.active_profile = "native";
    }
    active_profile_ = toUpperAscii(options_.active_profile);

    if (options_.enabled_feature_keys.empty()) {
        capability_feature_keys_ = defaultCapabilitySetForProfile(active_profile_);
    } else {
        capability_feature_keys_ = normalizeFeatureKeySet(options_.enabled_feature_keys);
    }

    const std::set<std::string> disabled = normalizeFeatureKeySet(options_.disabled_feature_keys);
    for (const std::string& key : disabled) {
        capability_feature_keys_.erase(key);
    }
}

bool Parser::requireFeature(const char* feature_key) {
    if (capability_feature_keys_.count(feature_key) != 0) {
        return true;
    }
    errorCode("PRS_0503", std::string("Feature ").append(feature_key).append(" not enabled for active profile"));
    return false;
}

// =============================================================================
// Error Handling
// =============================================================================

void Parser::error(const std::string& message) {
    errorAt(SourceSpan(currentLocation(), 1), message);
}

void Parser::error(const std::string& message, const std::string& hint) {
    errorAt(SourceSpan(currentLocation(), 1), message, hint);
}

void Parser::errorCode(const char* code, const std::string& message) {
    std::string coded;
    coded.reserve(std::char_traits<char>::length(code) + 2 + message.size());
    coded.append(code);
    coded.append(": ");
    coded.append(message);
    error(coded);
}

void Parser::errorAt(SourceSpan span, const std::string& message, const std::string& hint) {
    errors_.push_back({span, message, hint});
}

void Parser::synchronize() {
    advance();

    while (!isAtEnd()) {
        // Synchronize at statement boundaries
        if (previous().type == TokenType::SEMICOLON) {
            return;
        }

        // Or at statement starters
        switch (current().type) {
            case TokenType::KW_SELECT:
            case TokenType::KW_INSERT:
            case TokenType::KW_UPDATE:
            case TokenType::KW_DELETE:
            case TokenType::KW_CREATE:
            case TokenType::KW_ALTER:
            case TokenType::KW_DROP:
            case TokenType::KW_TRUNCATE:
            case TokenType::KW_GRANT:
            case TokenType::KW_REVOKE:
            case TokenType::KW_BEGIN:
            case TokenType::KW_PREPARE:
            case TokenType::KW_COMMIT:
            case TokenType::KW_ROLLBACK:
                return;
            default:
                break;
        }

        advance();
    }
}

// =============================================================================
// Utility Methods
// =============================================================================

bool Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) {
        advance();
        return true;
    }
    error(message);
    return false;
}

bool Parser::expectContextual(const char* keyword, const std::string& message) {
    if (matchContextual(keyword)) {
        return true;
    }
    error(message);
    return false;
}

StringPool::StringId Parser::currentIdentifier() {
    if (!isIdentifier()) {
        return StringPool::INVALID_ID;
    }
    StringPool::StringId id = state_.currentStringId();
    advance();
    return id;
}

StringPool::StringId Parser::expectIdentifier(const std::string& message) {
    if (!isIdentifier()) {
        error(message);
        return StringPool::INVALID_ID;
    }
    return currentIdentifier();
}

SourceSpan Parser::makeSpan(SourceLocation start) const {
    return SourceSpan(start,
        static_cast<uint32_t>(currentLocation().offset - start.offset));
}

// =============================================================================
// Main Parse Entry Points
// =============================================================================

ParseResult Parser::parseStatement() {
    ParseResult result;

    Statement* stmt = parseStatementInternal();
    result.setStatement(stmt);

    // Copy errors
    for (const auto& err : errors_) {
        result.addError(err);
    }

    return result;
}

ParseResult Parser::parsePsqlBody() {
    ParseModeGuard guard(state_, ParseMode::PSQL);
    ParseResult result;

    auto* block = arena_.create<ExecuteBlockStmt>();

    // Optional DECLARE section (DECLARE [VARIABLE] name type ...)
    while (check(TokenType::KW_DECLARE)) {
        advance();
        matchContextual("VARIABLE");

        VariableDecl var;
        var.name = expectIdentifier("Expected variable name");
        var.type = parseTypeName();

        if (match(TokenType::KW_NOT)) {
            expect(TokenType::KW_NULL, "Expected NULL after NOT");
            var.not_null = true;
        }

        if (match(TokenType::EQUAL) || match(TokenType::KW_DEFAULT)) {
            var.default_value = parseExpression();
        }

        block->variables.push_back(var);
        match(TokenType::SEMICOLON);
    }

    if (check(TokenType::KW_BEGIN)) {
        advance();
        block->body = parseBeginEndBlock();
        result.setStatement(block);
        return result;
    }

    // Fallback: single PSQL statement
    Statement* inner = parsePSQLStatement();
    if (!inner) {
        for (const auto& err : errors_) {
            result.addError(err);
        }
        return result;
    }

    auto* compound = arena_.create<CompoundStmt>();
    compound->statements.push_back(inner);
    block->body = compound;
    result.setStatement(block);
    return result;
}

std::vector<ParseResult> Parser::parseStatements() {
    std::vector<ParseResult> results;

    while (!isAtEnd()) {
        errors_.clear();
        ParseResult result = parseStatement();
        results.push_back(std::move(result));

        // Skip optional semicolon
        match(TokenType::SEMICOLON);
    }

    return results;
}

// =============================================================================
// Statement Dispatch
// =============================================================================

Statement* Parser::parseStatementInternal() {
    ParseModeGuard guard(state_, ParseMode::STATEMENT);

    if (check(TokenType::LEFT_BRACE)) {
        errorCode("PRS_0505",
                  "JDBC escape blocks ({fn ...}, {d ...}, {ts ...}) are not supported in v3; use canonical SQL forms");
        return nullptr;
    }
    if (checkContextual("REPLACE")) {
        errorCode("PRS_0505",
                  "REPLACE INTO is not supported in v3; use INSERT ... ON CONFLICT");
        return nullptr;
    }

    // Gatekeeper dispatch
    if (check(TokenType::KW_WITH))      return parseWithStatement();
    if (matchContextual("RECREATE"))    return parseRecreate();
    if (match(TokenType::KW_CREATE))    return parseCreate();
    if (match(TokenType::KW_ALTER))     return parseAlter();
    if (match(TokenType::KW_DROP))      return parseDrop();
    if (match(TokenType::KW_TRUNCATE))  return parseTruncate();
    if (match(TokenType::KW_DECLARE))   return parseDeclareTopLevel();
    if (checkContextual("FILTER")) {
        errorCode("PRS_0505",
                  "FILTER DOC PATH alias is not supported in v3; use DOC PATH FILTER");
        return nullptr;
    }
    if (checkContextual("AGGREGATE")) {
        errorCode("PRS_0505",
                  "AGGREGATE TIME BUCKET alias is not supported in v3; use TS BUCKET AGG");
        return nullptr;
    }
    if (checkContextual("ANN")) {
        errorCode("PRS_0505",
                  "ANN alias is not supported in v3; use VECTOR ANN QUERY");
        return nullptr;
    }
    if (checkContextual("CQL") || checkContextual("MONGO") ||
        checkContextual("CYPHER") || checkContextual("MILVUS")) {
        errorCode("PRS_0505",
                  "Engine-prefixed NoSQL aliases are not supported in v3");
        return nullptr;
    }
    if (checkContextual("EVAL") || checkContextual("XGROUP") ||
        checkContextual("XREADGROUP") || checkContextual("XCLAIM")) {
        errorCode("PRS_0505",
                  "Removed Redis alias surface is not supported in v3; use REDIS ... canonical commands");
        return nullptr;
    }
    if (checkContextual("DOC")) {
        if (!requireFeature(kFeatureDocPathFilter)) return nullptr;
        return parseDocPathFilterSurface();
    }
    if (checkContextual("TS")) {
        if (!requireFeature(kFeatureTsBucketAgg)) return nullptr;
        return parseTimeBucketAggSurface();
    }
    if (checkContextual("SEARCH") || check(TokenType::KW_JOIN) ||
        checkContextual("JOIN") || checkContextual("PERCOLATOR")) {
        if (!requireFeature(kFeatureSearchQueryDsl)) return nullptr;
        return parseSearchDslSurface();
    }
    if (checkContextual("VECTOR")) {
        if (!requireFeature(kFeatureVectorAnn)) return nullptr;
        return parseVectorAnnSurface();
    }
    if (checkContextual("GRAPH")) {
        return parseGraphPathSurface();
    }
    if (checkContextual("MATCH")) {
        Token lookahead = state_.lexer().peekToken();
        if (lookahead.type == TokenType::IDENTIFIER &&
            caseInsensitiveEquals(state_.lexer().getTokenText(lookahead.span), "GRAPH")) {
            return parseGraphPathSurface();
        }
    }
    if (checkContextual("REDIS")) {
        Token lookahead = state_.lexer().peekToken();
        if (lookahead.type != TokenType::END_OF_FILE &&
            caseInsensitiveEquals(state_.lexer().getTokenText(lookahead.span), "LUA")) {
            return parseRedisLuaEvalSurface();
        }
        if (lookahead.type != TokenType::END_OF_FILE &&
            caseInsensitiveEquals(state_.lexer().getTokenText(lookahead.span), "STREAM")) {
            return parseRedisStreamGroupSurface();
        }
        return parseNoSqlSurface();
    }
    if (checkContextual("HYBRID") || checkContextual("BRIDGE")) {
        if (!requireFeature(kFeatureHybridBridgeHint)) return nullptr;
        return parseHybridBridgeSurface();
    }
    if (checkContextual("COMPILE")) return parseUdrCompileSurface();
    if (checkContextual("UDR")) {
        Token lookahead = state_.lexer().peekToken();
        if (lookahead.type == TokenType::IDENTIFIER) {
            std::string_view next_text = state_.lexer().getTokenText(lookahead.span);
            if (caseInsensitiveEquals(next_text, "COMPILE") ||
                caseInsensitiveEquals(next_text, "VALIDATE")) {
                return parseUdrCompileSurface();
            }
        }
    }
    if (checkContextual("VALIDATE")) {
        Token lookahead = state_.lexer().peekToken();
        if (lookahead.type == TokenType::IDENTIFIER) {
            std::string_view next_text = state_.lexer().getTokenText(lookahead.span);
            if (caseInsensitiveEquals(next_text, "EMBEDDED") ||
                caseInsensitiveEquals(next_text, "SQL")) {
                return parseUdrCompileSurface();
            }
        }
    }
    if (matchContextual("INSTALL")) {
        return parseInstallExtensionSurface(false);
    }
    if (matchContextual("LOAD")) {
        return parseInstallExtensionSurface(true);
    }
    if (matchContextual("RESYNC")) {
        return parseResyncReplicationChannel();
    }
    if (matchContextual("BACKUP")) {
        return parseAdminControlSurface("BACKUP");
    }
    if (matchContextual("RESTORE")) {
        return parseAdminControlSurface("RESTORE");
    }
    if (matchContextual("VACUUM")) {
        errorCode("PRS_0505", "VACUUM is not supported in v3; use SWEEP DATABASE");
        return nullptr;
    }
    if (matchContextual("CHECKPOINT")) {
        return parseAdminControlSurface("CHECKPOINT");
    }
    if (matchContextual("WAIT")) {
        errorCode("PRS_0505",
                  "Top-level WAIT is not supported in v3; use transaction WAIT/NO WAIT controls");
        return nullptr;
    }
    if (matchContextual("REFRESH")) {
        return parseRefreshCubeControl();
    }
    if (matchContextual("CLUSTER")) {
        return parseClusterControlSurface();
    }
    if (matchContextual("CUBE")) {
        return parseCubeControlSurface();
    }
    if (matchContextual("SERVICE")) {
        return parseServiceChannelSurface();
    }

    // DML statements
    if (match(TokenType::KW_SELECT)) {
        if (checkContextual("COMPILE_EMBEDDED_PAYLOAD") ||
            checkContextual("VALIDATE_EMBEDDED_PAYLOAD")) {
            return parseSelectUdrCompileFunctionSurface();
        }
        return parseSelect();
    }
    if (match(TokenType::KW_INSERT))    return parseInsert();
    if (check(TokenType::KW_UPDATE)) {
        Token lookahead = state_.lexer().peekToken();
        if (lookahead.type == TokenType::KW_OR ||
            (lookahead.type == TokenType::IDENTIFIER &&
             caseInsensitiveEquals(state_.lexer().getTokenText(lookahead.span), "OR"))) {
            match(TokenType::KW_UPDATE);
            return parseUpdateOrInsert();
        }
    }
    if (match(TokenType::KW_UPDATE))    return parseUpdate();
    if (match(TokenType::KW_DELETE))    return parseDelete();
    if (match(TokenType::KW_COPY))      return parseCopy();

    // Transaction statements
    if (match(TokenType::KW_BEGIN))     return parseStartTransaction();
    if (match(TokenType::KW_START))     return parseStartTransaction();
    if (match(TokenType::KW_PREPARE))   return parsePrepareTransaction();
    if (match(TokenType::KW_COMMIT))    return parseCommit();
    if (match(TokenType::KW_ROLLBACK))  return parseRollback();
    if (matchContextual("SAVEPOINT"))   return parseSavepoint();
    if (matchContextual("RELEASE"))     return parseReleaseSavepoint();

    // Session statements
    if (match(TokenType::KW_SET))       return parseSet();
    if (check(TokenType::KW_SHOW)) {
        Token lookahead = state_.lexer().peekToken();
        if (lookahead.type != TokenType::END_OF_FILE) {
            if (caseInsensitiveEquals(state_.lexer().getTokenText(lookahead.span), "CLUSTER")) {
                match(TokenType::KW_SHOW);
                return parseShowClusterControlSurface();
            }
            if (caseInsensitiveEquals(state_.lexer().getTokenText(lookahead.span), "CUBE")) {
                match(TokenType::KW_SHOW);
                return parseShowCubeControlSurface();
            }
        }
        match(TokenType::KW_SHOW);
        return parseShow();
    }
    if (matchContextual("RESET"))       return parseReset();
    if (matchContextual("DESCRIBE")) return parseDescribe();
    if (matchContextual("DESC")) {
        errorCode("PRS_0505", "DESC alias is not supported in v3; use DESCRIBE");
        return nullptr;
    }
    if (checkContextual("SECURITY")) {
        matchContextual("SECURITY");
        if (matchContextual("LABEL")) {
            return parseSecurityLabel();
        }
        error("Expected LABEL after SECURITY");
        return nullptr;
    }

    // Utility statements
    if (match(TokenType::KW_EXPLAIN))   return parseExplain();
    if (match(TokenType::KW_ANALYZE))   return parseAnalyze();
    if (matchContextual("VALIDATE")) {
        if (checkContextual("INDEX")) {
            return parseValidateIndex();
        }
        return parseAdminControlSurface("VALIDATE");
    }
    if (matchContextual("SWEEP"))       return parseSweep();
    if (matchContextual("CANCEL")) {
        if (matchContextual("JOB")) {
            return parseCancelJobRun();
        }
        error("Expected JOB after CANCEL");
        return nullptr;
    }
    if (check(TokenType::KW_EXECUTE)) {
        match(TokenType::KW_EXECUTE);
        if (matchContextual("JOB")) {
            return parseExecuteJob();
        }
        return parseExecuteStatement();
    }
    if (match(TokenType::KW_CALL) || matchContextual("CALL")) {
        return parseCall();
    }

    // DCL statements
    if (match(TokenType::KW_GRANT))     return parseGrant();
    if (check(TokenType::KW_REVOKE)) {
        Token lookahead = state_.lexer().peekToken();
        if (lookahead.type == TokenType::IDENTIFIER) {
            std::string_view next_text = state_.lexer().getTokenText(lookahead.span);
            auto eq_ci = [&](std::string_view a, std::string_view b) {
                if (a.size() != b.size()) {
                    return false;
                }
                for (size_t i = 0; i < a.size(); ++i) {
                    if (std::toupper(static_cast<unsigned char>(a[i])) !=
                        std::toupper(static_cast<unsigned char>(b[i]))) {
                        return false;
                    }
                }
                return true;
            };
            if (eq_ci(next_text, "TOKEN")) {
                if (!requireFeature(kFeatureSecurityTokenDdl)) return nullptr;
                match(TokenType::KW_REVOKE);
                return parseRevokeToken();
            }
        }
    }
    if (match(TokenType::KW_REVOKE))    return parseRevoke();

    // Connection statements
    if (matchContextual("CONNECT"))     return parseConnect();
    if (matchContextual("DISCONNECT"))  return parseDisconnect();

    // Metadata statements
    if (matchContextual("COMMENT"))     return parseComment();

    // MERGE statement
    if (match(TokenType::KW_MERGE) || matchContextual("MERGE")) return parseMerge();

    error("Expected SQL statement");
    return nullptr;
}

Statement* Parser::parseRecreate() {
    ParseModeGuard guard(state_, ParseMode::DDL);

    if (matchContextual("JOB")) {
        return parseCreateJob(false, true);
    }
    if (matchContextual("TABLE")) {
        return parseCreateTable(true, TempTableType::NONE);
    }
    if (matchContextual("VIEW")) {
        return parseCreateView(true);
    }
    if (matchContextual("PROCEDURE")) {
        return parseCreateProcedure(true);
    }
    if (matchContextual("FUNCTION")) {
        return parseCreateFunction(true);
    }
    if (matchContextual("TRIGGER")) {
        return parseCreateTrigger(true);
    }
    if (matchContextual("PACKAGE")) {
        return parseCreatePackage(true);
    }
    if (matchContextual("EXCEPTION")) {
        return parseCreateException(true);
    }
    if (matchContextual("SEQUENCE")) {
        auto* stmt = parseCreateSequence();
        if (stmt) {
            stmt->or_replace = true;
        }
        return stmt;
    }

    errorCode("PRS_0505", "Unsupported RECREATE object type");
    return nullptr;
}

Statement* Parser::parseDeclareTopLevel() {
    if (matchContextual("EXTERNAL")) {
        if (matchContextual("FUNCTION")) {
            return parseDeclareExternalFunction();
        }
        errorCode("PRS_0505", "Expected FUNCTION after DECLARE EXTERNAL");
        return nullptr;
    }

    errorCode("PRS_0505", "Unsupported DECLARE statement");
    return nullptr;
}

CreateUdrStmt* Parser::parseDeclareExternalFunction() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<CreateUdrStmt>();

    stmt->udr_type = UdrObjectType::FUNCTION;
    stmt->udr_path = parseSchemaPath(state_);
    if (stmt->udr_path.isEmpty()) {
        errorCode("PRS_0505", "Expected function name after DECLARE EXTERNAL FUNCTION");
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (match(TokenType::LEFT_PAREN)) {
        int depth = 1;
        while (!isAtEnd() && depth > 0) {
            if (match(TokenType::LEFT_PAREN)) {
                ++depth;
            } else if (match(TokenType::RIGHT_PAREN)) {
                --depth;
            } else {
                advance();
            }
        }
    } else {
        while (!isAtEnd() && !checkContextual("RETURNS") && !check(TokenType::SEMICOLON)) {
            advance();
        }
    }

    if (matchContextual("RETURNS")) {
        if (matchContextual("PARAMETER")) {
            // Firebird legacy: RETURNS PARAMETER n
            if (check(TokenType::INTEGER_LITERAL)) {
                advance();
            }
        } else {
            (void)parseTypeName();
            if (matchContextual("BY")) {
                matchContextual("VALUE");
                matchContextual("DESCRIPTOR");
            }
            if (matchContextual("FREE_IT")) {
                // Optional Firebird marker; ignored in normalized AST.
            }
        }
    }

    expectContextual("ENTRY_POINT", "Expected ENTRY_POINT in DECLARE EXTERNAL FUNCTION");
    if (!check(TokenType::STRING_LITERAL)) {
        errorCode("PRS_0505", "Expected string literal for ENTRY_POINT");
        stmt->span = makeSpan(start);
        return stmt;
    }
    stmt->entry_point = std::string(stringPool().get(current().value.string_id));
    advance();

    expectContextual("MODULE_NAME", "Expected MODULE_NAME in DECLARE EXTERNAL FUNCTION");
    if (!check(TokenType::STRING_LITERAL)) {
        errorCode("PRS_0505", "Expected string literal for MODULE_NAME");
        stmt->span = makeSpan(start);
        return stmt;
    }
    stmt->library_path = std::string(stringPool().get(current().value.string_id));
    advance();

    stmt->signature = "DECLARE EXTERNAL FUNCTION";
    stmt->has_signature = true;
    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// CREATE Statement Dispatch
// =============================================================================

Statement* Parser::parseCreate() {
    ParseModeGuard guard(state_, ParseMode::DDL);

    // Check for OR REPLACE / OR ALTER
    bool or_replace = false;
    bool or_alter = false;
    if (check(TokenType::KW_OR) || checkContextual("OR")) {
        match(TokenType::KW_OR) || matchContextual("OR");
        if (matchContextual("REPLACE")) {
            or_replace = true;
        } else if (match(TokenType::KW_ALTER) || matchContextual("ALTER")) {
            or_alter = true;
        } else {
            error("Expected REPLACE or ALTER after OR");
        }
    }

    // Check for UNIQUE (for CREATE UNIQUE INDEX)
    bool unique = false;
    if (checkContextual("UNIQUE")) {
        matchContextual("UNIQUE");
        unique = true;
    }

    // Check for TEMPORARY/TEMP (including GLOBAL TEMPORARY for Firebird-style GTT)
    bool temporary = false;
    TempTableType temp_type = TempTableType::NONE;
    if (checkContextual("GLOBAL")) {
        matchContextual("GLOBAL");
        if (checkContextual("TEMPORARY") || checkContextual("TEMP")) {
            matchContextual("TEMPORARY") || matchContextual("TEMP");
            temporary = true;
            temp_type = TempTableType::GLOBAL;
        } else {
            error("Expected TEMPORARY after GLOBAL");
        }
    } else if (checkContextual("TEMPORARY") || checkContextual("TEMP")) {
        matchContextual("TEMPORARY") || matchContextual("TEMP");
        temporary = true;
        temp_type = TempTableType::SESSION;
    }

    // Check for UNLOGGED
    bool unlogged = false;
    if (checkContextual("UNLOGGED")) {
        matchContextual("UNLOGGED");
        unlogged = true;
    }

    // CREATE MATERIALIZED VIEW prefix form is removed in v3 canonical grammar.
    // Materialization must be expressed via explicit CREATE/ALTER VIEW options.
    bool materialized = false;
    if (checkContextual("MATERIALIZED")) {
        errorCode("PRS_0505",
                  "CREATE MATERIALIZED VIEW is not supported in v3; use CREATE VIEW ... MATERIALIZED");
        return nullptr;
    }

    // Dispatch based on object type

    if (matchContextual("MEASUREMENT") || matchContextual("SCHEDULE")) {
        errorCode("PRS_0505",
                  "Top-level CREATE MEASUREMENT/SCHEDULE is not supported in v3; use CREATE JOB ...");
        return nullptr;
    }
    if (matchContextual("SEARCH")) {
        if (matchContextual("INDEX")) {
            errorCode("PRS_0505",
                      "CREATE SEARCH INDEX is not supported in v3; use CREATE INDEX ... USING FULLTEXT");
        } else {
            errorCode("PRS_0505", "Unsupported SEARCH create surface");
        }
        return nullptr;
    }
    if (matchContextual("VECTOR")) {
        if (matchContextual("INDEX")) {
            errorCode("PRS_0505",
                      "CREATE VECTOR INDEX is not supported in v3; use CREATE INDEX ... USING <vector_method>");
        } else {
            errorCode("PRS_0505", "Unsupported VECTOR create surface");
        }
        return nullptr;
    }

    if (matchContextual("CONNECTION")) {
        if (!matchContextual("RULE")) {
            errorCode("PRS_0505", "Expected RULE after CREATE CONNECTION");
            return nullptr;
        }
        if (or_alter) {
            errorCode("PRS_0505", "CREATE OR ALTER is not supported for CONNECTION RULE");
        }
        if (!requireFeature(kFeatureSecurityConnectionRuleDdl)) {
            return nullptr;
        }
        return parseCreateConnectionRule();
    }

    if (matchContextual("TOKEN")) {
        if (or_alter) {
            errorCode("PRS_0505", "CREATE OR ALTER is not supported for TOKEN");
        }
        if (!requireFeature(kFeatureSecurityTokenDdl)) {
            return nullptr;
        }
        return parseCreateToken();
    }

    if (matchContextual("QUOTA")) {
        if (!matchContextual("PROFILE")) {
            errorCode("PRS_0505", "Expected PROFILE after CREATE QUOTA");
            return nullptr;
        }
        if (or_alter) {
            errorCode("PRS_0505", "CREATE OR ALTER is not supported for QUOTA PROFILE");
        }
        if (!requireFeature(kFeatureSecurityQuotaProfileDdl)) {
            return nullptr;
        }
        return parseCreateQuotaProfile();
    }
    if (matchContextual("EXTENSION")) {
        if (or_alter) {
            errorCode("PRS_0505", "CREATE OR ALTER is not supported for EXTENSION");
        }
        return parseCreateExtension();
    }
    if (matchContextual("REPLICATION")) {
        if (!matchContextual("CHANNEL")) {
            errorCode("PRS_0505", "Expected CHANNEL after CREATE REPLICATION");
            return nullptr;
        }
        if (or_alter) {
            errorCode("PRS_0505", "CREATE OR ALTER is not supported for REPLICATION CHANNEL");
        }
        return parseCreateReplicationChannel();
    }
    if (matchContextual("PUBLICATION")) {
        if (or_alter) {
            errorCode("PRS_0505", "CREATE OR ALTER is not supported for PUBLICATION");
        }
        return parseCreatePublication();
    }
    if (matchContextual("SUBSCRIPTION")) {
        if (or_alter) {
            errorCode("PRS_0505", "CREATE OR ALTER is not supported for SUBSCRIPTION");
        }
        return parseCreateSubscription();
    }
    if (matchContextual("ACCESS")) {
        if (!matchContextual("METHOD")) {
            errorCode("PRS_0505", "Expected METHOD after CREATE ACCESS");
            return nullptr;
        }
        if (or_alter) {
            errorCode("PRS_0505", "CREATE OR ALTER is not supported for ACCESS METHOD");
        }
        return parseCreateAccessMethod();
    }
    if (matchContextual("STATISTICS")) {
        if (or_alter) {
            errorCode("PRS_0505", "CREATE OR ALTER is not supported for STATISTICS");
        }
        return parseCreateStatistics();
    }
    if (matchContextual("TRANSFORM")) {
        if (or_alter) {
            errorCode("PRS_0505", "CREATE OR ALTER is not supported for TRANSFORM");
        }
        return parseCreateTransform();
    }

    if (matchContextual("CLUSTER")) {
        if (or_alter) {
            errorCode("PRS_0505", "CREATE OR ALTER is not supported for CLUSTER controls");
        }
        return parseCreateClusterControl();
    }

    if (matchContextual("CUBE")) {
        if (or_alter) {
            errorCode("PRS_0505", "CREATE OR ALTER is not supported for CUBE controls");
        }
        return parseCreateCubeControl();
    }

    if (matchContextual("SCHEMA")) {
        if (or_alter) {
            error("CREATE OR ALTER is only supported for JOB");
        }
        return parseCreateSchema();
    }

    if (matchContextual("DATABASE")) {
        if (matchContextual("CONNECTION")) {
            if (or_alter) {
                errorCode("PRS_0505", "CREATE OR ALTER is not supported for DATABASE CONNECTION");
            }
            return parseCreateDatabaseConnection();
        }
        if (or_alter) {
            error("CREATE OR ALTER is only supported for JOB");
        }
        return parseCreateDatabase();
    }

    if (matchContextual("CDC")) {
        if (!matchContextual("TABLE")) {
            errorCode("PRS_0505", "Expected TABLE after CREATE CDC");
            return nullptr;
        }
        if (or_alter) {
            errorCode("PRS_0505", "CREATE OR ALTER is not supported for CDC TABLE");
        }
        return parseCreateCdcTable();
    }

    if (matchContextual("TABLESPACE")) {
        if (or_alter) {
            error("CREATE OR ALTER is only supported for JOB");
        }
        return parseCreateTablespace();
    }

    if (matchContextual("DOMAIN")) {
        if (or_alter) {
            error("CREATE OR ALTER is only supported for JOB");
        }
        return parseCreateDomain();
    }

    if (matchContextual("TABLE")) {
        if (or_alter) {
            error("CREATE OR ALTER is only supported for JOB");
        }
        auto* stmt = parseCreateTable(or_replace, temp_type);
        if (stmt) {
            stmt->temp_type = temp_type;
            stmt->unlogged = unlogged;
        }
        return stmt;
    }

    if (matchContextual("INDEX")) {
        if (or_alter) {
            error("CREATE OR ALTER is only supported for JOB");
        }
        auto* stmt = parseCreateIndex();
        if (stmt) {
            stmt->unique = unique;
        }
        return stmt;
    }

    if (matchContextual("VIEW")) {
        auto* stmt = parseCreateView(or_replace || or_alter);
        if (stmt) {
            stmt->temporary = temporary;
            // Prefix form is rejected in v3. Preserve/merge view-level option parsing.
            stmt->materialized = stmt->materialized || materialized;
        }
        return stmt;
    }

    if (matchContextual("SEQUENCE")) {
        if (or_alter) {
            error("CREATE OR ALTER is only supported for JOB");
        }
        auto* stmt = parseCreateSequence();
        if (stmt) {
            stmt->or_replace = or_replace;
            stmt->temporary = temporary;
        }
        return stmt;
    }

    if (matchContextual("FUNCTION")) {
        return parseCreateFunction(or_replace || or_alter);
    }
    if (matchContextual("PROCEDURE")) {
        return parseCreateProcedure(or_replace || or_alter);
    }
    if (matchContextual("TRIGGER")) {
        return parseCreateTrigger(or_replace || or_alter);
    }
    if (matchContextual("PACKAGE")) {
        return parseCreatePackage(or_replace || or_alter);
    }
    if (matchContextual("EXCEPTION")) {
        return parseCreateException(or_replace || or_alter);
    }
    if (matchContextual("TYPE")) {
        if (or_alter) {
            error("CREATE OR ALTER is only supported for JOB");
        }
        return parseCreateType(or_replace);
    }
    if (matchContextual("USER")) {
        if (or_alter) {
            error("CREATE OR ALTER is only supported for JOB");
        }
        if (matchContextual("MAPPING")) {
            return parseCreateUserMapping();
        }
        if (!requireFeature(kFeatureSecurityUserAccountDdl)) {
            return nullptr;
        }
        return parseCreateUser();
    }
    if (matchContextual("ROLE")) {
        if (or_alter) {
            error("CREATE OR ALTER is only supported for JOB");
        }
        return parseCreateRole();
    }
    if (matchContextual("GROUP")) {
        if (or_alter) {
            error("CREATE OR ALTER is only supported for JOB");
        }
        return parseCreateGroup();
    }
    if (matchContextual("POLICY")) {
        if (or_alter) {
            error("CREATE OR ALTER is only supported for JOB");
        }
        if (!requireFeature(kFeatureSecurityModelPolicyDdl)) {
            return nullptr;
        }
        return parseCreatePolicy();
    }
    if (matchContextual("SERVER")) {
        if (or_alter) {
            error("CREATE OR ALTER is only supported for JOB");
        }
        return parseCreateForeignServer();
    }
    if (matchContextual("FOREIGN")) {
        if (matchContextual("TABLE")) {
            if (or_alter) {
                error("CREATE OR ALTER is only supported for JOB");
            }
            return parseCreateForeignTable();
        }
        if (matchContextual("DATA")) {
            expectContextual("WRAPPER", "Expected WRAPPER after FOREIGN DATA");
            if (or_alter) {
                error("CREATE OR ALTER is only supported for JOB");
            }
            return parseCreateForeignDataWrapper();
        }
        error("Expected TABLE or DATA WRAPPER after FOREIGN");
        return nullptr;
    }
    if (matchContextual("PUBLIC")) {
        if (matchContextual("SYNONYM")) {
            if (or_alter) {
                error("CREATE OR ALTER is only supported for JOB");
            }
            return parseCreateSynonym(true);
        }
        error("Expected SYNONYM after PUBLIC");
        return nullptr;
    }
    if (matchContextual("SYNONYM")) {
        if (or_alter) {
            error("CREATE OR ALTER is only supported for JOB");
        }
        return parseCreateSynonym(false);
    }
    if (matchContextual("UDR")) {
        if (or_alter) {
            error("CREATE OR ALTER is only supported for JOB");
        }
        return parseCreateUdr();
    }
    if (matchContextual("JOB")) {
        if (or_replace) {
            error("CREATE OR REPLACE is not supported for JOB");
        }
        return parseCreateJob(or_alter, false);
    }

    error("Expected object type after CREATE (TABLE, INDEX, VIEW, SEQUENCE, ROLE, USER, ...)");
    return nullptr;
}

// =============================================================================
// CREATE USER / ROLE
// =============================================================================

CreateUserStmt* Parser::parseCreateUser() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateUserStmt>();
    stmt->user_name = expectIdentifier("Expected user name");

    if (match(TokenType::KW_WITH) || matchContextual("WITH")) {
        // Optional WITH before options
    }

    while (true) {
        if (matchContextual("PASSWORD")) {
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal for PASSWORD");
                break;
            }
            stmt->has_password = true;
            stmt->password = current().value.string_id;
            advance();
        } else if (matchContextual("SUPERUSER")) {
            stmt->is_superuser = true;
        } else if (matchContextual("NOSUPERUSER")) {
            stmt->is_superuser = false;
        } else if (match(TokenType::KW_WITH) || matchContextual("WITH")) {
            continue;
        } else {
            break;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

CreateRoleStmt* Parser::parseCreateRole() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateRoleStmt>();
    stmt->role_name = expectIdentifier("Expected role name");
    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// CREATE JOB
// =============================================================================

CreateJobStmt* Parser::parseCreateJob(bool or_alter, bool recreate) {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateJobStmt>();
    stmt->or_alter = or_alter;
    stmt->recreate = recreate;
    stmt->job_name = expectIdentifier("Expected job name");

    auto parse_duration_seconds = [&](const char* context) -> uint32_t {
        if (!check(TokenType::INTEGER_LITERAL)) {
            error(std::string("Expected duration seconds for ") + context);
            return 0;
        }
        int64_t value = current().value.int_value;
        advance();
        if (value < 0) {
            error(std::string("Duration must be non-negative for ") + context);
            return 0;
        }

        uint64_t multiplier = 1;
        if (isIdentifier()) {
            std::string unit = std::string(stringPool().get(current().value.string_id));
            advance();
            if (caseInsensitiveEquals(unit, "S") || caseInsensitiveEquals(unit, "SEC") ||
                caseInsensitiveEquals(unit, "SECOND") || caseInsensitiveEquals(unit, "SECONDS")) {
                multiplier = 1;
            } else if (caseInsensitiveEquals(unit, "M") || caseInsensitiveEquals(unit, "MIN") ||
                       caseInsensitiveEquals(unit, "MINUTE") || caseInsensitiveEquals(unit, "MINUTES")) {
                multiplier = 60;
            } else if (caseInsensitiveEquals(unit, "H") || caseInsensitiveEquals(unit, "HOUR") ||
                       caseInsensitiveEquals(unit, "HOURS")) {
                multiplier = 3600;
            } else if (caseInsensitiveEquals(unit, "D") || caseInsensitiveEquals(unit, "DAY") ||
                       caseInsensitiveEquals(unit, "DAYS")) {
                multiplier = 86400;
            } else {
                error(std::string("Unknown duration unit for ") + context);
                return static_cast<uint32_t>(value);
            }
        }

        uint64_t seconds = static_cast<uint64_t>(value) * multiplier;
        if (seconds > std::numeric_limits<uint32_t>::max()) {
            error(std::string("Duration too large for ") + context);
            return std::numeric_limits<uint32_t>::max();
        }
        return static_cast<uint32_t>(seconds);
    };

    auto parse_timestamp_literal = [&](const char* context) -> StringPool::StringId {
        if (!check(TokenType::STRING_LITERAL)) {
            error(std::string("Expected timestamp string for ") + context);
            return StringPool::INVALID_ID;
        }
        auto id = current().value.string_id;
        advance();
        return id;
    };

    auto canonicalize_rrule = [&](StringPool::StringId raw_id,
                                  StringPool::StringId& canonical_id) -> bool {
        if (raw_id == StringPool::INVALID_ID) {
            return false;
        }

        auto trim = [](std::string_view s) -> std::string_view {
            size_t b = 0;
            while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) {
                ++b;
            }
            size_t e = s.size();
            while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
                --e;
            }
            return s.substr(b, e - b);
        };

        std::set<std::string> seen_keys;
        std::vector<std::pair<std::string, std::string>> kv_pairs;
        std::string raw = std::string(stringPool().get(raw_id));
        size_t pos = 0;
        while (pos < raw.size()) {
            size_t next = raw.find(';', pos);
            std::string token = std::string(trim(std::string_view(raw).substr(
                pos, next == std::string::npos ? std::string::npos : next - pos)));
            if (token.empty()) {
                errorCode("PRS_0507", "Invalid RRULE token");
                return false;
            }
            size_t eq = token.find('=');
            if (eq == std::string::npos || eq == 0 || eq + 1 >= token.size()) {
                errorCode("PRS_0507", "Invalid RRULE key/value contract");
                return false;
            }

            std::string key = std::string(trim(token.substr(0, eq)));
            std::string value = std::string(trim(token.substr(eq + 1)));
            for (char& c : key) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }

            static const char* kAllowed[] = {
                "FREQ", "INTERVAL", "COUNT", "UNTIL", "BYSECOND", "BYMINUTE", "BYHOUR",
                "BYDAY", "BYMONTHDAY", "BYYEARDAY", "BYWEEKNO", "BYMONTH", "BYSETPOS", "WKST"
            };
            bool allowed = false;
            for (const char* candidate : kAllowed) {
                if (key == candidate) {
                    allowed = true;
                    break;
                }
            }
            if (!allowed) {
                errorCode("PRS_0507", "Invalid RRULE key");
                return false;
            }
            if (!seen_keys.insert(key).second) {
                errorCode("PRS_0507", "Duplicate RRULE key");
                return false;
            }

            kv_pairs.push_back({std::move(key), std::move(value)});
            if (next == std::string::npos) {
                break;
            }
            pos = next + 1;
        }

        if (seen_keys.find("FREQ") == seen_keys.end()) {
            errorCode("PRS_0507", "RRULE requires FREQ");
            return false;
        }
        if (seen_keys.size() < 2) {
            errorCode("PRS_0507",
                      "RRULE requires at least one scheduling constraint beyond FREQ");
            return false;
        }

        std::sort(kv_pairs.begin(), kv_pairs.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

        std::string canonical;
        for (size_t i = 0; i < kv_pairs.size(); ++i) {
            if (i != 0) {
                canonical.push_back(';');
            }
            canonical.append(kv_pairs[i].first);
            canonical.push_back('=');
            canonical.append(kv_pairs[i].second);
        }
        canonical_id = stringPool().intern(canonical);
        return true;
    };

    auto parse_schedule = [&]() -> bool {
        expectContextual("SCHEDULE", "Expected SCHEDULE");
        expect(TokenType::EQUAL, "Expected '=' after SCHEDULE");

        if (matchContextual("CRON")) {
            if (!requireFeature(kFeatureScheduleRruleSurface)) {
                return false;
            }
            stmt->schedule_kind = JobScheduleKind::CRON;
            auto raw = parse_timestamp_literal("CRON");
            auto canonical = raw;
            canonicalize_rrule(raw, canonical);
            stmt->cron_expression = canonical;
            return true;
        }
        if (matchContextual("AT")) {
            stmt->schedule_kind = JobScheduleKind::AT;
            stmt->at_timestamp = parse_timestamp_literal("AT");
            return true;
        }
        if (matchContextual("EVERY")) {
            stmt->schedule_kind = JobScheduleKind::EVERY;
            stmt->interval_seconds = parse_duration_seconds("EVERY");
            if (matchContextual("STARTS")) {
                stmt->starts_at = parse_timestamp_literal("STARTS");
            }
            if (matchContextual("ENDS")) {
                stmt->ends_at = parse_timestamp_literal("ENDS");
            }
            return true;
        }

        error("Expected CRON, AT, or EVERY after SCHEDULE");
        return false;
    };

    auto parse_partition_list = [&](const char* context) -> StringPool::StringId {
        expect(TokenType::LEFT_PAREN, std::string("Expected '(' after ").append(context));
        std::string combined;
        bool first = true;
        while (!check(TokenType::RIGHT_PAREN) && !check(TokenType::END_OF_FILE)) {
            if (!check(TokenType::STRING_LITERAL) && !check(TokenType::IDENTIFIER)) {
                error(std::string("Expected shard identifier or string for ").append(context));
                break;
            }
            auto text = stringPool().get(current().value.string_id);
            advance();
            if (!first) {
                combined.push_back(',');
            }
            combined.append(text);
            first = false;
            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, std::string("Expected ')' after ").append(context));
        if (combined.empty()) {
            error(std::string(context).append(" requires at least one shard"));
            return StringPool::INVALID_ID;
        }
        return stringPool().intern(combined);
    };

    auto parse_partition_expression = [&](const char* context) -> StringPool::StringId {
        if (check(TokenType::STRING_LITERAL) || check(TokenType::IDENTIFIER)) {
            auto id = current().value.string_id;
            advance();
            return id;
        }
        if (match(TokenType::LEFT_PAREN)) {
            if (check(TokenType::RIGHT_PAREN)) {
                error(std::string(context).append(" expression cannot be empty"));
                advance();
                return StringPool::INVALID_ID;
            }

            std::string_view input = state_.lexer().input();
            size_t start = current().span.start.offset;
            size_t end = start;
            bool saw_token = false;
            int depth = 1;
            Token last = current();

            while (!isAtEnd()) {
                if (check(TokenType::LEFT_PAREN)) {
                    depth++;
                } else if (check(TokenType::RIGHT_PAREN)) {
                    depth--;
                    if (depth == 0) {
                        break;
                    }
                }
                last = current();
                saw_token = true;
                advance();
            }

            if (saw_token) {
                end = last.span.start.offset + last.span.length;
                if (end > input.size()) {
                    end = input.size();
                }
            }

            expect(TokenType::RIGHT_PAREN, std::string("Expected ')' after ").append(context));
            if (!saw_token || end <= start) {
                error(std::string(context).append(" expression cannot be empty"));
                return StringPool::INVALID_ID;
            }

            std::string_view text = input.substr(start, end - start);
            size_t trim_start = text.find_first_not_of(" \t\r\n");
            if (trim_start == std::string_view::npos) {
                error(std::string(context).append(" expression cannot be empty"));
                return StringPool::INVALID_ID;
            }
            size_t trim_end = text.find_last_not_of(" \t\r\n");
            return stringPool().intern(text.substr(trim_start, trim_end - trim_start + 1));
        }

        error(std::string("Expected expression for ").append(context));
        return StringPool::INVALID_ID;
    };

    auto parse_measurement_key = [&]() -> StringPool::StringId {
        if (matchContextual("ENABLED")) {
            return stringPool().intern("ENABLED");
        }
        if (matchContextual("WINDOW")) {
            return stringPool().intern("WINDOW");
        }
        if (matchContextual("RETENTION")) {
            return stringPool().intern("RETENTION");
        }
        if (matchContextual("GRANULARITY")) {
            return stringPool().intern("GRANULARITY");
        }
        if (!isIdentifier()) {
            error("Expected MEASUREMENT option key");
            return StringPool::INVALID_ID;
        }
        auto key = current().value.string_id;
        advance();
        return key;
    };

    auto parse_measurement_value = [&]() -> StringPool::StringId {
        if (check(TokenType::COMMA) || check(TokenType::RIGHT_PAREN)) {
            error("Expected MEASUREMENT option value");
            return StringPool::INVALID_ID;
        }

        std::string_view input = state_.lexer().input();
        size_t start = current().span.start.offset;
        size_t end = start;
        bool saw_token = false;
        int depth = 0;
        Token last = current();

        while (!isAtEnd()) {
            if (check(TokenType::COMMA) && depth == 0) {
                break;
            }
            if (check(TokenType::RIGHT_PAREN) && depth == 0) {
                break;
            }
            if (check(TokenType::LEFT_PAREN)) {
                depth++;
            } else if (check(TokenType::RIGHT_PAREN) && depth > 0) {
                depth--;
            }
            last = current();
            saw_token = true;
            advance();
        }

        if (saw_token) {
            end = last.span.start.offset + last.span.length;
            if (end > input.size()) {
                end = input.size();
            }
        }

        if (!saw_token || end <= start) {
            error("Expected MEASUREMENT option value");
            return StringPool::INVALID_ID;
        }

        std::string_view text = input.substr(start, end - start);
        size_t trim_start = text.find_first_not_of(" \t\r\n");
        if (trim_start == std::string_view::npos) {
            error("Expected MEASUREMENT option value");
            return StringPool::INVALID_ID;
        }
        size_t trim_end = text.find_last_not_of(" \t\r\n");
        return stringPool().intern(text.substr(trim_start, trim_end - trim_start + 1));
    };

    auto parse_measurement_clause = [&]() {
        stmt->has_measurement = true;
        stmt->measurement_options.clear();
        expect(TokenType::LEFT_PAREN, "Expected '(' after MEASUREMENT");
        if (check(TokenType::RIGHT_PAREN)) {
            error("MEASUREMENT clause must include at least one option");
        }
        while (!check(TokenType::RIGHT_PAREN) && !check(TokenType::END_OF_FILE)) {
            auto key = parse_measurement_key();
            expect(TokenType::EQUAL, "Expected '=' after MEASUREMENT option key");
            auto value = parse_measurement_value();
            stmt->measurement_options.emplace_back(key, value);
            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after MEASUREMENT options");
        if (stmt->measurement_options.empty()) {
            error("MEASUREMENT clause must include at least one option");
        }
    };

    bool has_schedule = false;

    while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
        if (check(TokenType::KW_AS) || check(TokenType::KW_CALL) ||
            check(TokenType::KW_EXECUTE) ||
            checkContextual("AS") || checkContextual("CALL") || checkContextual("EXEC")) {
            break;
        }

        if (checkContextual("SCHEDULE")) {
            has_schedule = parse_schedule() || has_schedule;
        } else if (matchContextual("MEASUREMENT")) {
            parse_measurement_clause();
        } else if (matchContextual("DEPENDS")) {
            if (!(match(TokenType::KW_ON) || matchContextual("ON"))) {
                error("Expected ON after DEPENDS");
            }
            do {
                stmt->depends_on.push_back(expectIdentifier("Expected dependency job name"));
            } while (match(TokenType::COMMA));
        } else if (matchContextual("CLASS")) {
            expect(TokenType::EQUAL, "Expected '=' after CLASS");
            stmt->job_class = expectIdentifier("Expected job class");
        } else if (matchContextual("PARTITION")) {
            expectContextual("BY", "Expected BY after PARTITION");
            if (matchContextual("ALL_SHARDS")) {
                stmt->partition_strategy = stringPool().intern("ALL_SHARDS");
            } else if (matchContextual("SINGLE_SHARD")) {
                stmt->partition_strategy = stringPool().intern("SINGLE_SHARD");
                stmt->partition_shard = parse_timestamp_literal("SINGLE_SHARD");
            } else if (matchContextual("SHARD_SET")) {
                stmt->partition_strategy = stringPool().intern("SHARD_SET");
                if (check(TokenType::LEFT_PAREN)) {
                    stmt->partition_expression = parse_partition_list("SHARD_SET");
                } else {
                    stmt->partition_expression = parse_partition_expression("SHARD_SET");
                }
            } else if (matchContextual("DYNAMIC")) {
                stmt->partition_strategy = stringPool().intern("DYNAMIC");
                stmt->partition_expression = parse_partition_expression("DYNAMIC");
            } else {
                error("Expected partition strategy after PARTITION BY");
            }
        } else if (matchContextual("MAX_RETRIES")) {
            expect(TokenType::EQUAL, "Expected '=' after MAX_RETRIES");
            if (!check(TokenType::INTEGER_LITERAL)) {
                error("Expected integer value for MAX_RETRIES");
            } else {
                stmt->has_max_retries = true;
                stmt->max_retries = static_cast<uint32_t>(current().value.int_value);
                advance();
            }
        } else if (matchContextual("RETRY_BACKOFF")) {
            expect(TokenType::EQUAL, "Expected '=' after RETRY_BACKOFF");
            stmt->has_retry_backoff = true;
            stmt->retry_backoff_seconds = parse_duration_seconds("RETRY_BACKOFF");
        } else if (matchContextual("TIMEOUT")) {
            expect(TokenType::EQUAL, "Expected '=' after TIMEOUT");
            stmt->has_timeout = true;
            stmt->timeout_seconds = parse_duration_seconds("TIMEOUT");
        } else if (match(TokenType::KW_ON) || matchContextual("ON")) {
            if (!matchContextual("COMPLETION")) {
                error("Expected COMPLETION after ON");
            }
            stmt->has_on_completion = true;
            if (matchContextual("PRESERVE")) {
                stmt->on_completion = JobOnCompletion::PRESERVE;
            } else if (match(TokenType::KW_DROP) || matchContextual("DROP")) {
                stmt->on_completion = JobOnCompletion::DROP;
            } else {
                error("Expected PRESERVE or DROP after ON COMPLETION");
            }
        } else if (matchContextual("RUN")) {
            if (!(match(TokenType::KW_AS) || matchContextual("AS"))) {
                error("Expected AS after RUN");
            }
            stmt->has_run_as = true;
            stmt->run_as_role = expectIdentifier("Expected role name after RUN AS");
        } else if (matchContextual("DESCRIPTION")) {
            expect(TokenType::EQUAL, "Expected '=' after DESCRIPTION");
            stmt->has_description = true;
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal for DESCRIPTION");
            } else {
                stmt->description = current().value.string_id;
                advance();
            }
        } else if (matchContextual("STATE")) {
            expect(TokenType::EQUAL, "Expected '=' after STATE");
            stmt->has_state = true;
            if (matchContextual("ENABLED")) {
                stmt->state = JobState::ENABLED;
            } else if (matchContextual("DISABLED")) {
                stmt->state = JobState::DISABLED;
            } else if (matchContextual("PAUSED")) {
                stmt->state = JobState::PAUSED;
            } else {
                error("Expected ENABLED, DISABLED, or PAUSED after STATE");
            }
        } else if (matchContextual("ENABLED")) {
            stmt->has_state = true;
            stmt->state = JobState::ENABLED;
        } else if (matchContextual("DISABLED")) {
            stmt->has_state = true;
            stmt->state = JobState::DISABLED;
        } else if (matchContextual("PAUSED")) {
            stmt->has_state = true;
            stmt->state = JobState::PAUSED;
        } else {
            break;
        }
    }

    if (!has_schedule) {
        error("CREATE JOB requires SCHEDULE");
    }

    if (match(TokenType::KW_AS) || matchContextual("AS")) {
        if (matchContextual("SQL")) {
            // Optional SQL keyword in canonical CREATE JOB body form.
        }
        if (!check(TokenType::STRING_LITERAL)) {
            error("Expected SQL string after AS");
        } else {
            stmt->job_type = JobType::SQL;
            stmt->job_sql = current().value.string_id;
            advance();
        }
    } else if (match(TokenType::KW_CALL) || matchContextual("CALL")) {
        stmt->job_type = JobType::PROCEDURE;
        SchemaPath proc_path = parseSchemaPath(state_);
        if (proc_path.isEmpty()) {
            error("Expected procedure name after CALL");
        } else {
            std::string proc_name = schemaPathToString(proc_path, stringPool());
            stmt->procedure_name = stringPool().intern(proc_name);
        }
        if (match(TokenType::LEFT_PAREN)) {
            expect(TokenType::RIGHT_PAREN, "Expected ')' after CALL procedure");
        }
    } else if (match(TokenType::KW_EXECUTE) || matchContextual("EXEC")) {
        stmt->job_type = JobType::EXTERNAL;
        if (!check(TokenType::STRING_LITERAL)) {
            error("Expected command string after EXEC");
        } else {
            stmt->external_command = current().value.string_id;
            advance();
        }
    } else {
        error("Expected AS, CALL, or EXEC for job definition");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreateMeasurement() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateTableStmt>();
    stmt->table_path = parseSchemaPath(state_);
    if (stmt->table_path.isEmpty()) {
        error("Expected measurement name");
    }

    expect(TokenType::LEFT_PAREN, "Expected '(' after measurement name");
    if (check(TokenType::RIGHT_PAREN)) {
        errorCode("PRS_0504", "CREATE MEASUREMENT field list must not be empty");
    }

    bool first = true;
    while (!check(TokenType::RIGHT_PAREN) &&
           !check(TokenType::SEMICOLON) &&
           !check(TokenType::END_OF_FILE)) {
        if (!first) {
            expect(TokenType::COMMA, "Expected ',' between measurement fields");
        }
        first = false;
        auto* column = parseColumnDef();
        if (column) {
            stmt->columns.push_back(column);
        } else {
            break;
        }
    }
    expect(TokenType::RIGHT_PAREN, "Expected ')' after measurement fields");

    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreateSchedule() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateJobStmt>();
    stmt->job_name = expectIdentifier("Expected schedule name");
    stmt->job_type = JobType::SQL;
    stmt->job_sql = stringPool().intern("SELECT 1");

    auto trim = [](std::string_view s) -> std::string_view {
        size_t b = 0;
        while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) {
            ++b;
        }
        size_t e = s.size();
        while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
            --e;
        }
        return s.substr(b, e - b);
    };

    auto parse_string_lit = [&](const char* context) -> std::string {
        if (!check(TokenType::STRING_LITERAL)) {
            errorCode("PRS_0507", std::string("Expected string literal for ") + context);
            return {};
        }
        auto value = std::string(stringPool().get(current().value.string_id));
        advance();
        return value;
    };

    auto parse_rrule = [&](const std::string& raw_in, std::string& canonical_out) -> bool {
        std::set<std::string> seen_keys;
        std::vector<std::pair<std::string, std::string>> kv_pairs;

        std::string raw(raw_in);
        size_t pos = 0;
        while (pos < raw.size()) {
            size_t next = raw.find(';', pos);
            std::string token = std::string(trim(std::string_view(raw).substr(
                pos, next == std::string::npos ? std::string::npos : next - pos)));
            if (token.empty()) {
                errorCode("PRS_0507", "Invalid RRULE token");
                return false;
            }
            size_t eq = token.find('=');
            if (eq == std::string::npos || eq == 0 || eq + 1 >= token.size()) {
                errorCode("PRS_0507", "Invalid RRULE key/value contract");
                return false;
            }
            std::string key = token.substr(0, eq);
            std::string value = token.substr(eq + 1);
            key = std::string(trim(key));
            value = std::string(trim(value));
            for (char& c : key) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }

            static const char* kAllowed[] = {
                "FREQ", "INTERVAL", "COUNT", "UNTIL", "BYSECOND", "BYMINUTE", "BYHOUR",
                "BYDAY", "BYMONTHDAY", "BYYEARDAY", "BYWEEKNO", "BYMONTH", "BYSETPOS", "WKST"
            };
            bool allowed = false;
            for (const char* candidate : kAllowed) {
                if (key == candidate) {
                    allowed = true;
                    break;
                }
            }
            if (!allowed) {
                errorCode("PRS_0507", "Invalid RRULE key");
                return false;
            }
            if (!seen_keys.insert(key).second) {
                errorCode("PRS_0507", "Duplicate RRULE key");
                return false;
            }
            kv_pairs.push_back({std::move(key), std::move(value)});

            if (next == std::string::npos) {
                break;
            }
            pos = next + 1;
        }

        if (seen_keys.find("FREQ") == seen_keys.end()) {
            errorCode("PRS_0507", "RRULE requires FREQ");
            return false;
        }

        std::sort(kv_pairs.begin(), kv_pairs.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

        std::string canonical;
        for (size_t i = 0; i < kv_pairs.size(); ++i) {
            if (i != 0) {
                canonical.push_back(';');
            }
            canonical.append(kv_pairs[i].first);
            canonical.push_back('=');
            canonical.append(kv_pairs[i].second);
        }
        canonical_out = std::move(canonical);
        return true;
    };

    auto parse_local_ts_list = [&](const char* context, std::vector<std::string>& out) -> bool {
        if (!expect(TokenType::LEFT_PAREN, std::string("Expected '(' after ").append(context))) {
            return false;
        }
        if (check(TokenType::RIGHT_PAREN)) {
            errorCode("PRS_0507", std::string(context) + " list must not be empty");
            advance();
            return false;
        }
        while (!check(TokenType::RIGHT_PAREN) &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            out.push_back(parse_string_lit(context));
            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, std::string("Expected ')' after ").append(context));
        return !out.empty();
    };

    std::string recurrence;
    if (matchContextual("RRULE")) {
        std::string raw_rrule = parse_string_lit("RRULE");
        std::string canonical_rrule;
        if (parse_rrule(raw_rrule, canonical_rrule)) {
            recurrence = "RRULE " + canonical_rrule;
        }
    } else if (matchContextual("RRULE_SET")) {
        if (!expect(TokenType::LEFT_PAREN, "Expected '(' after RRULE_SET")) {
            stmt->span = makeSpan(start);
            return stmt;
        }
        std::set<std::string> unique_rules;
        while (!check(TokenType::RIGHT_PAREN) &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            std::string raw_rrule = parse_string_lit("RRULE_SET item");
            std::string canonical_rrule;
            if (parse_rrule(raw_rrule, canonical_rrule)) {
                unique_rules.insert(canonical_rrule);
            }
            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after RRULE_SET");
        if (unique_rules.size() < 2) {
            errorCode("PRS_0507", "RRULE_SET requires at least two unique RRULE members");
        }
        recurrence = "RRULE_SET(";
        bool first = true;
        for (const auto& member : unique_rules) {
            if (!first) {
                recurrence.push_back(',');
            }
            recurrence.append(member);
            first = false;
        }
        recurrence.push_back(')');
    } else {
        errorCode("PRS_0507", "Expected RRULE or RRULE_SET");
    }

    if (!matchContextual("DTSTART")) {
        errorCode("PRS_0507", "Missing DTSTART");
    }
    std::string dtstart = parse_string_lit("DTSTART");

    if (!matchContextual("TZ")) {
        errorCode("PRS_0507", "Missing TZ");
    }
    std::string timezone = parse_string_lit("TZ");

    std::vector<std::string> rdate_list;
    std::vector<std::string> exdate_list;
    while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
        if (matchContextual("RDATE")) {
            parse_local_ts_list("RDATE", rdate_list);
            continue;
        }
        if (matchContextual("EXDATE")) {
            parse_local_ts_list("EXDATE", exdate_list);
            continue;
        }
        errorCode("PRS_0508", "Unsupported recurrence-source token");
        break;
    }

    std::string payload = recurrence;
    payload.append(" DTSTART=");
    payload.append(dtstart);
    payload.append(" TZ=");
    payload.append(timezone);
    if (!rdate_list.empty()) {
        payload.append(" RDATE=(");
        for (size_t i = 0; i < rdate_list.size(); ++i) {
            if (i != 0) payload.push_back(',');
            payload.append(rdate_list[i]);
        }
        payload.push_back(')');
    }
    if (!exdate_list.empty()) {
        payload.append(" EXDATE=(");
        for (size_t i = 0; i < exdate_list.size(); ++i) {
            if (i != 0) payload.push_back(',');
            payload.append(exdate_list[i]);
        }
        payload.push_back(')');
    }

    stmt->schedule_kind = JobScheduleKind::CRON;
    stmt->cron_expression = stringPool().intern(payload);
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreateConnectionRule() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    StringPool::StringId rule_name = expectIdentifier("Expected connection rule name");
    std::string rule = std::string(stringPool().get(rule_name));
    auto match_word = [&](TokenType token, const char* word) {
        return match(token) || matchContextual(word);
    };

    auto parse_block = [&](const char* context) -> std::string {
        if (!expect(TokenType::LEFT_PAREN, std::string("Expected '(' after ").append(context))) {
            return {};
        }
        std::string_view input = state_.lexer().input();
        size_t block_start = current().span.start.offset;
        size_t block_end = block_start;
        int depth = 1;
        Token last = current();
        bool saw_token = false;
        while (!isAtEnd()) {
            if (check(TokenType::LEFT_PAREN)) {
                ++depth;
            } else if (check(TokenType::RIGHT_PAREN)) {
                --depth;
                if (depth == 0) {
                    break;
                }
            }
            last = current();
            saw_token = true;
            advance();
        }
        expect(TokenType::RIGHT_PAREN, std::string("Expected ')' after ").append(context));
        if (!saw_token) {
            return {};
        }
        block_end = last.span.start.offset + last.span.length;
        if (block_end > input.size()) {
            block_end = input.size();
        }
        return std::string(input.substr(block_start, block_end - block_start));
    };

    if (!match_word(TokenType::KW_ORDER, "ORDER") || !check(TokenType::INTEGER_LITERAL)) {
        errorCode("SEC_1235", "CREATE CONNECTION RULE requires ORDER <uint32>");
    } else {
        advance();
    }

    if (!matchContextual("MATCH")) {
        errorCode("SEC_1235", "CREATE CONNECTION RULE requires MATCH (...)");
    }
    std::string match_block = parse_block("MATCH");

    if (!matchContextual("REQUIRE")) {
        errorCode("SEC_1235", "CREATE CONNECTION RULE requires REQUIRE (...)");
    }
    std::string require_block = parse_block("REQUIRE");

    if (!matchContextual("ACTION")) {
        errorCode("SEC_1235", "CREATE CONNECTION RULE requires ACTION");
    } else if (!(matchContextual("ALLOW") || matchContextual("DENY"))) {
        errorCode("SEC_1235", "ACTION must be ALLOW or DENY");
    }

    if (matchContextual("MAP")) {
        if (matchContextual("AUTH")) {
            expectContextual("POLICY", "Expected POLICY after MAP AUTH");
            expectIdentifier("Expected policy name");
        } else if (matchContextual("DEFAULT")) {
            expectContextual("ROLE", "Expected ROLE after MAP DEFAULT");
            expectIdentifier("Expected role name");
        } else {
            errorCode("SEC_1235", "Unsupported MAP clause in CREATE CONNECTION RULE");
        }
    }

    if (matchContextual("REJECT")) {
        expectContextual("CODE", "Expected CODE after REJECT");
        expectIdentifier("Expected reject code");
    }

    if (!matchContextual("EXPECT") ||
        !matchContextual("VERSION") ||
        !check(TokenType::INTEGER_LITERAL)) {
        errorCode("SEC_1235", "CREATE CONNECTION RULE requires EXPECT VERSION <uint64>");
    } else {
        advance();
    }

    stmt->name = stringPool().intern("security.connection_rule.create." + rule);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern("MATCH(" + match_block + ") REQUIRE(" + require_block + ")");
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreateToken() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    StringPool::StringId token_name = expectIdentifier("Expected token name");
    std::string token = std::string(stringPool().get(token_name));

    if (match(TokenType::KW_WITH) || matchContextual("WITH")) {
        // optional WITH
    }

    std::string scope_model = "GENERIC";
    if (matchContextual("SCOPE_MODEL")) {
        if (!isIdentifier()) {
            errorCode("SEC_1256", "Expected SCOPE_MODEL identifier");
        } else {
            scope_model = std::string(stringPool().get(currentIdentifier()));
            std::string normalized = scope_model;
            for (char& c : normalized) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            if (!(normalized == "GENERIC" ||
                  normalized == "INFLUX" ||
                  normalized == "MILVUS" ||
                  normalized == "OPENSEARCH" ||
                  normalized == "CLICKHOUSE")) {
                errorCode("SEC_1256", "Invalid token scope model");
            }
            scope_model = normalized;
        }
    }

    if (!matchContextual("SCOPE")) {
        errorCode("SEC_1256", "CREATE TOKEN requires SCOPE clause");
    }

    if (!expect(TokenType::LEFT_PAREN, "Expected '(' after SCOPE")) {
        stmt->span = makeSpan(start);
        return stmt;
    }
    std::string_view input = state_.lexer().input();
    size_t block_start = current().span.start.offset;
    size_t block_end = block_start;
    int depth = 1;
    bool saw_token = false;
    Token last = current();
    while (!isAtEnd()) {
        if (check(TokenType::LEFT_PAREN)) {
            ++depth;
        } else if (check(TokenType::RIGHT_PAREN)) {
            --depth;
            if (depth == 0) {
                break;
            }
        }
        last = current();
        saw_token = true;
        advance();
    }
    expect(TokenType::RIGHT_PAREN, "Expected ')' after token scope");
    if (!saw_token) {
        errorCode("SEC_1256", "Token scope payload must not be empty");
    }
    block_end = last.span.start.offset + last.span.length;
    if (block_end > input.size()) {
        block_end = input.size();
    }

    std::string payload = "SCOPE_MODEL=" + scope_model + " SCOPE(";
    if (block_end > block_start) {
        payload.append(input.substr(block_start, block_end - block_start));
    }
    payload.push_back(')');

    stmt->name = stringPool().intern("security.token.create." + token);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreateQuotaProfile() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    StringPool::StringId profile_name = expectIdentifier("Expected quota profile name");
    std::string profile = std::string(stringPool().get(profile_name));

    if (!expect(TokenType::LEFT_PAREN, "Expected '(' after quota profile name")) {
        stmt->span = makeSpan(start);
        return stmt;
    }
    std::string_view input = state_.lexer().input();
    size_t block_start = current().span.start.offset;
    size_t block_end = block_start;
    int depth = 1;
    bool saw_token = false;
    Token last = current();
    while (!isAtEnd()) {
        if (check(TokenType::LEFT_PAREN)) {
            ++depth;
        } else if (check(TokenType::RIGHT_PAREN)) {
            --depth;
            if (depth == 0) {
                break;
            }
        }
        last = current();
        saw_token = true;
        advance();
    }
    expect(TokenType::RIGHT_PAREN, "Expected ')' after quota profile options");
    if (!saw_token) {
        errorCode("PRS_0504", "CREATE QUOTA PROFILE option list must not be empty");
    }
    block_end = last.span.start.offset + last.span.length;
    if (block_end > input.size()) {
        block_end = input.size();
    }

    stmt->name = stringPool().intern("security.quota_profile.create." + profile);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(
        block_end > block_start ? std::string(input.substr(block_start, block_end - block_start))
                                : std::string());
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreateExtension() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    bool if_not_exists = false;
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        if_not_exists = true;
    }

    StringPool::StringId extension_name = expectIdentifier("Expected extension name");
    std::string extension = std::string(stringPool().get(extension_name));
    std::string payload = captureStatementBody();
    if (if_not_exists) {
        if (!payload.empty()) payload.insert(0, ";");
        payload.insert(0, "IF_NOT_EXISTS=1");
    }

    stmt->name = stringPool().intern("platform.extension.create." + extension);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreateReplicationChannel() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    bool if_not_exists = false;
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        if_not_exists = true;
    }

    StringPool::StringId channel_name = expectIdentifier("Expected replication channel name");
    std::string channel = std::string(stringPool().get(channel_name));
    std::string payload = captureStatementBody();

    if (payload.empty()) {
        errorCode("PRS_0504", "CREATE REPLICATION CHANNEL requires channel configuration");
    }

    const std::string payload_upper = toUpperAscii(payload);
    const bool has_one_way = payload_upper.find("ONE_WAY") != std::string::npos ||
                             payload_upper.find("ONEWAY") != std::string::npos;
    const bool has_bidirectional = payload_upper.find("BIDIRECTIONAL") != std::string::npos ||
                                   payload_upper.find("TWO_WAY") != std::string::npos;
    if (!has_one_way && !has_bidirectional) {
        errorCode("PRS_0504",
                  "CREATE REPLICATION CHANNEL requires DIRECTION ONE_WAY or DIRECTION BIDIRECTIONAL");
    }
    if (has_one_way && has_bidirectional) {
        errorCode("PRS_0504", "REPLICATION CHANNEL direction must be either ONE_WAY or BIDIRECTIONAL");
    }

    if (if_not_exists) {
        if (!payload.empty()) payload.insert(0, ";");
        payload.insert(0, "IF_NOT_EXISTS=1");
    }

    stmt->name = stringPool().intern("replication.channel.create." + channel);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreatePublication() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    StringPool::StringId publication_name = expectIdentifier("Expected publication name");
    std::string publication = std::string(stringPool().get(publication_name));

    std::string payload = captureStatementBody();
    if (payload.empty()) {
        errorCode("PRS_0504", "CREATE PUBLICATION requires FOR clause");
    }

    stmt->name = stringPool().intern("replication.publication.create." + publication);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreateSubscription() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    StringPool::StringId subscription_name = expectIdentifier("Expected subscription name");
    std::string subscription = std::string(stringPool().get(subscription_name));

    std::string payload = captureStatementBody();
    if (payload.empty()) {
        errorCode("PRS_0504", "CREATE SUBSCRIPTION requires CONNECTION/PUBLICATION clauses");
    }

    stmt->name = stringPool().intern("replication.subscription.create." + subscription);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreateCdcTable() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    bool if_not_exists = false;
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        if_not_exists = true;
    }

    SchemaPath table_path = parseSchemaPath(state_);
    if (table_path.isEmpty()) {
        errorCode("PRS_0504", "Expected CDC table name");
    }

    std::string table_name = schemaPathToString(table_path, stringPool());
    std::string payload = captureStatementBody();
    const std::string payload_upper = toUpperAscii(payload);
    const bool has_track = payload_upper.find("TRACK") != std::string::npos;
    const bool has_last_modified = payload_upper.find("LAST_MODIFIED_TXN_ID") != std::string::npos ||
                                   payload_upper.find("LAST_EDIT_TXID") != std::string::npos;
    const bool has_row_uuid = payload_upper.find("ROW_UUID") != std::string::npos;
    if (!has_track || !has_last_modified || !has_row_uuid) {
        errorCode("PRS_0504",
                  "CREATE CDC TABLE requires TRACK (LAST_MODIFIED_TXN_ID, ROW_UUID)");
    }

    if (if_not_exists) {
        if (!payload.empty()) payload.insert(0, ";");
        payload.insert(0, "IF_NOT_EXISTS=1");
    }

    stmt->name = stringPool().intern("etl.cdc.table.create." + table_name);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreateAccessMethod() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    StringPool::StringId method_name = expectIdentifier("Expected access method name");
    std::string method = std::string(stringPool().get(method_name));

    expectContextual("TYPE", "Expected TYPE after access method name");
    if (!(matchContextual("INDEX") || matchContextual("TABLE"))) {
        errorCode("PRS_0504", "CREATE ACCESS METHOD requires TYPE INDEX or TYPE TABLE");
    }
    expectContextual("HANDLER", "Expected HANDLER in CREATE ACCESS METHOD");
    expectIdentifier("Expected handler function name");

    std::string payload = captureStatementBody();
    stmt->name = stringPool().intern("platform.access_method.create." + method);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreateStatistics() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    bool if_not_exists = false;
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        if_not_exists = true;
    }

    StringPool::StringId stats_name = expectIdentifier("Expected statistics object name");
    std::string stats = std::string(stringPool().get(stats_name));
    std::string payload = captureStatementBody();
    if (if_not_exists) {
        if (!payload.empty()) payload.insert(0, ";");
        payload.insert(0, "IF_NOT_EXISTS=1");
    }

    stmt->name = stringPool().intern("optimizer.statistics.create." + stats);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreateTransform() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    expectContextual("FOR", "Expected FOR after CREATE TRANSFORM");
    TypeName source_type = parseTypeName();
    if (!source_type.has_schema_path && source_type.name == StringPool::INVALID_ID) {
        errorCode("PRS_0504", "CREATE TRANSFORM requires source type");
    }

    std::string payload = "FOR ";
    if (source_type.has_schema_path) {
        payload += schemaPathToString(source_type.schema_path, stringPool());
    } else if (source_type.name != StringPool::INVALID_ID) {
        payload += std::string(stringPool().get(source_type.name));
    } else {
        payload += "<unknown>";
    }
    std::string tail = captureStatementBody();
    if (!tail.empty()) {
        payload.push_back(' ');
        payload.append(tail);
    }

    stmt->name = stringPool().intern("platform.transform.create");
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// CREATE TABLE
// =============================================================================

CreateTableStmt* Parser::parseCreateTable(bool or_replace, TempTableType temp_type) {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateTableStmt>();
    stmt->or_replace = or_replace;
    stmt->temp_type = temp_type;

    // Check for IF NOT EXISTS (IF, NOT, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    // Parse table name (schema path)
    stmt->table_path = parseSchemaPath(state_);
    if (stmt->table_path.isEmpty()) {
        error("Expected table name");
        return stmt;
    }

    // Parse column definitions and constraints (optional for CREATE TABLE AS SELECT)
    if (match(TokenType::LEFT_PAREN)) {
        ParseModeGuard colGuard(state_, ParseMode::COLUMN_DEF);

        // Parse comma-separated list of columns and constraints
        bool first = true;
        while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
            if (!first) {
                if (!expect(TokenType::COMMA, "Expected ',' between column definitions")) {
                    break;
                }
            }
            first = false;

            // Check if this is a table constraint
            if (checkContextual("CONSTRAINT") ||
                checkContextual("PRIMARY") ||
                checkContextual("UNIQUE") ||
                checkContextual("FOREIGN") ||
                checkContextual("CHECK")) {
                TableConstraint* constraint = parseTableConstraint();
                if (constraint) {
                    stmt->constraints.push_back(constraint);
                }
            } else {
                // Column definition
                ColumnDef* col = parseColumnDef();
                if (col) {
                    stmt->columns.push_back(col);
                }
            }
        }

        expect(TokenType::RIGHT_PAREN, "Expected ')' after column definitions");
    } else if (!check(TokenType::KW_AS) && !checkContextual("AS") &&
               !check(TokenType::KW_SELECT) && !check(TokenType::KW_WITH)) {
        error("Expected '(' or AS/SELECT after table name");
        return stmt;
    }

    // Parse optional table options
    bool has_on_commit = false;
    while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
        if (check(TokenType::KW_ON) || checkContextual("ON")) {
            if (has_on_commit) {
                error("ON COMMIT specified more than once");
            }
            if (!(match(TokenType::KW_ON) || matchContextual("ON"))) {
                error("Expected ON");
            }
            if (!(match(TokenType::KW_COMMIT) || matchContextual("COMMIT"))) {
                error("Expected COMMIT after ON");
            }
            if (match(TokenType::KW_DELETE) || matchContextual("DELETE")) {
                stmt->on_commit = TempOnCommitAction::DELETE_ROWS;
                matchContextual("ROWS");
            } else if (matchContextual("PRESERVE")) {
                stmt->on_commit = TempOnCommitAction::PRESERVE_ROWS;
                matchContextual("ROWS");
            } else if (match(TokenType::KW_DROP) || matchContextual("DROP")) {
                stmt->on_commit = TempOnCommitAction::DROP;
            } else {
                error("Expected DELETE, PRESERVE, or DROP after ON COMMIT");
            }
            has_on_commit = true;
        } else if (checkContextual("TABLESPACE")) {
            matchContextual("TABLESPACE");
            stmt->tablespace = parseSchemaPath(state_);
            stmt->has_tablespace = true;
        } else if (checkContextual("INHERITS")) {
            matchContextual("INHERITS");
            expect(TokenType::LEFT_PAREN, "Expected '(' after INHERITS");
            // Parse parent tables
            do {
                stmt->inherits.push_back(parseSchemaPath(state_));
            } while (match(TokenType::COMMA));
            expect(TokenType::RIGHT_PAREN, "Expected ')' after parent table list");
        } else if (checkContextual("PARTITION")) {
            matchContextual("PARTITION");
            expectContextual("BY", "Expected BY after PARTITION");
            stmt->is_partitioned = true;
            // Parse partition type
            if (matchContextual("RANGE") || matchContextual("LIST") || matchContextual("HASH")) {
                stmt->partition_by = previous().value.string_id;
            }
            expect(TokenType::LEFT_PAREN, "Expected '(' after partition type");
            // Parse partition columns
            do {
                stmt->partition_columns.push_back(expectIdentifier("Expected partition column name"));
            } while (match(TokenType::COMMA));
            expect(TokenType::RIGHT_PAREN, "Expected ')' after partition columns");
        } else {
            break;  // Unknown option, stop parsing
        }
    }

    if (match(TokenType::KW_AS) || matchContextual("AS")) {
        if (check(TokenType::KW_WITH)) {
            stmt->as_query = parseSelectWithClause();
        } else if (match(TokenType::KW_SELECT)) {
            stmt->as_query = parseSelect();
        } else {
            error("Expected SELECT after AS in CREATE TABLE");
        }
    } else if (check(TokenType::KW_SELECT) || check(TokenType::KW_WITH)) {
        if (check(TokenType::KW_WITH)) {
            stmt->as_query = parseSelectWithClause();
        } else if (match(TokenType::KW_SELECT)) {
            stmt->as_query = parseSelect();
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// Column Definition
// =============================================================================

ColumnDef* Parser::parseColumnDef() {
    SourceLocation start = currentLocation();

    auto* col = arena_.create<ColumnDef>();

    // Column name
    col->name = expectIdentifier("Expected column name");
    if (col->name == StringPool::INVALID_ID) {
        return col;
    }

    // Data type
    col->type = parseTypeName();

    // Column constraints
    col->constraints = parseColumnConstraints();

    col->span = makeSpan(start);
    return col;
}

// =============================================================================
// Type Name Parsing
// =============================================================================

TypeName Parser::parseTypeName() {
    ParseModeGuard guard(state_, ParseMode::TYPE_NAME);
    SourceLocation start = currentLocation();

    TypeName type;

    // Firebird TYPE OF / TYPE OF COLUMN
    if (checkContextual("TYPE")) {
        matchContextual("TYPE");
        expectContextual("OF", "Expected OF after TYPE");
        type.is_type_of = true;
        if (matchContextual("COLUMN")) {
            type.is_type_of_column = true;
            type.name = stringPool().intern("TYPE OF COLUMN");
        } else {
            type.name = stringPool().intern("TYPE OF");
        }
        type.schema_path = parseSchemaPath(state_);
        type.has_schema_path = !type.schema_path.isEmpty();
        if (!type.has_schema_path) {
            error("Expected type or column reference after TYPE OF");
        }
        type.span = makeSpan(start);
        return type;
    }

    if (state_.check(TokenType::DOT) || state_.check(TokenType::DOUBLE_DOT) ||
        state_.check(TokenType::EXCLAIM_COLON)) {
        type.schema_path = parseSchemaPath(state_);
        type.has_schema_path = true;
        type.span = makeSpan(start);
        return type;
    }

    // Type name (contextual keyword)
    if (!isIdentifier()) {
        error("Expected data type name");
        return type;
    }

    Token next = state_.lexer().peekToken();
    if (next.type == TokenType::DOT) {
        type.schema_path = parseSchemaPath(state_);
        type.has_schema_path = true;
        type.span = makeSpan(start);
        return type;
    }

    type.name = currentIdentifier();

    // Handle two-word type names
    std::string_view type_text = stringPool().get(type.name);
    auto to_upper = [](std::string_view s) {
        std::string result;
        result.reserve(s.size());
        for (char c : s) result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return result;
    };
    std::string upper_type = to_upper(type_text);

    // DOUBLE PRECISION -> FLOAT64/DOUBLE
    if (upper_type == "DOUBLE") {
        if (matchContextual("PRECISION")) {
            // "DOUBLE PRECISION" - combine into a single type name
            type.name = stringPool().intern("DOUBLE PRECISION");
        }
    }
    // CHARACTER VARYING -> VARCHAR (alternative syntax)
    else if (upper_type == "CHARACTER") {
        if (matchContextual("VARYING")) {
            type.name = stringPool().intern("VARCHAR");
        }
    }
    // BIT VARYING -> VARBIT
    else if (upper_type == "BIT") {
        if (matchContextual("VARYING")) {
            type.name = stringPool().intern("VARBIT");
        }
    }

    // Check for type parameters: numeric (precision/scale) or generic argument lists.
    if (match(TokenType::LEFT_PAREN)) {
        if (check(TokenType::INTEGER_LITERAL)) {
            type.length = static_cast<int32_t>(current().value.int_value);
            type.precision = type.length;
            advance();

            if (match(TokenType::COMMA)) {
                if (check(TokenType::INTEGER_LITERAL)) {
                    type.scale = static_cast<int32_t>(current().value.int_value);
                    advance();
                } else {
                    error("Expected integer literal for second numeric type parameter");
                }
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after type parameters");
        } else {
            std::string_view input = state_.lexer().input();
            auto trim = [](std::string_view v) -> std::string_view {
                size_t b = 0;
                while (b < v.size() && std::isspace(static_cast<unsigned char>(v[b])) != 0) {
                    ++b;
                }
                size_t e = v.size();
                while (e > b && std::isspace(static_cast<unsigned char>(v[e - 1])) != 0) {
                    --e;
                }
                return v.substr(b, e - b);
            };
            auto appendTypeArgument = [&](size_t begin, size_t end) {
                if (end <= begin || begin >= input.size()) {
                    return;
                }
                size_t safe_end = std::min(end, input.size());
                std::string_view raw = trim(input.substr(begin, safe_end - begin));
                if (!raw.empty()) {
                    type.type_arguments.push_back(stringPool().intern(std::string(raw)));
                }
            };

            int nested_depth = 0;
            size_t arg_start = current().span.start.offset;
            bool saw_closing_paren = false;
            while (!isAtEnd()) {
                if (check(TokenType::LEFT_PAREN)) {
                    ++nested_depth;
                    advance();
                    continue;
                }
                if (check(TokenType::RIGHT_PAREN)) {
                    if (nested_depth == 0) {
                        appendTypeArgument(arg_start, current().span.start.offset);
                        advance();
                        saw_closing_paren = true;
                        break;
                    }
                    --nested_depth;
                    advance();
                    continue;
                }
                if (check(TokenType::COMMA) && nested_depth == 0) {
                    appendTypeArgument(arg_start, current().span.start.offset);
                    advance();
                    if (!isAtEnd()) {
                        arg_start = current().span.start.offset;
                    }
                    continue;
                }
                advance();
            }

            if (!saw_closing_paren) {
                error("Expected ')' after type arguments");
            }
            if (type.type_arguments.empty()) {
                error("Type argument list must not be empty");
            }
        }
    }

    // Check for array notation: []
    if (match(TokenType::LEFT_BRACKET)) {
        type.is_array = true;
        if (check(TokenType::INTEGER_LITERAL)) {
            type.array_size = static_cast<int32_t>(current().value.int_value);
            advance();
        }
        expect(TokenType::RIGHT_BRACKET, "Expected ']' after array size");
    }

    auto peek_contextual = [&](const Token& token, const char* keyword) -> bool {
        if (token.type != TokenType::IDENTIFIER) {
            return false;
        }
        return caseInsensitiveEquals(stringPool().get(token.value.string_id), keyword);
    };

    // Check for WITH/WITHOUT TIME ZONE (don't consume unrelated WITH tokens)
    if (check(TokenType::KW_WITH)) {
        Token next = state_.lexer().peekToken();
        if (peek_contextual(next, "TIME")) {
            match(TokenType::KW_WITH);
            expectContextual("TIME", "Expected TIME after WITH");
            expectContextual("ZONE", "Expected ZONE after WITH TIME");
            type.with_time_zone = true;
        }
    } else if (state_.checkContextual("WITHOUT")) {
        Token next = state_.lexer().peekToken();
        if (peek_contextual(next, "TIME")) {
            matchContextual("WITHOUT");
            expectContextual("TIME", "Expected TIME after WITHOUT");
            matchContextual("ZONE");
            type.with_time_zone = false;
        }
    }

    type.span = makeSpan(start);
    return type;
}

// =============================================================================
// Column Constraints
// =============================================================================

std::vector<ColumnConstraint> Parser::parseColumnConstraints() {
    std::vector<ColumnConstraint> constraints;

    while (true) {
        // Check for CONSTRAINT name
        StringPool::StringId constraint_name = StringPool::INVALID_ID;
        if (matchContextual("CONSTRAINT")) {
            constraint_name = expectIdentifier("Expected constraint name");
        }

        ColumnConstraint constraint;
        constraint.name = constraint_name;
        bool found = false;

        if (match(TokenType::KW_NOT)) {
            expect(TokenType::KW_NULL, "Expected NULL after NOT");
            constraint.type = ConstraintType::NOT_NULL;
            found = true;
        } else if (match(TokenType::KW_NULL)) {
            constraint.type = ConstraintType::NULL_ALLOWED;
            found = true;
        } else if (matchContextual("PRIMARY")) {
            expectContextual("KEY", "Expected KEY after PRIMARY");
            constraint.type = ConstraintType::PRIMARY_KEY;
            found = true;
        } else if (matchContextual("UNIQUE")) {
            constraint.type = ConstraintType::UNIQUE;
            found = true;
        } else if (matchContextual("CHECK")) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after CHECK");
            constraint.type = ConstraintType::CHECK;
            constraint.check_expr = parseExpression();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after CHECK expression");
            found = true;
        } else if (match(TokenType::KW_DEFAULT)) {
            constraint.type = ConstraintType::DEFAULT;
            constraint.default_expr = parseExpression();
            found = true;
        } else if (matchContextual("REFERENCES")) {
            constraint.type = ConstraintType::REFERENCES;
            constraint.ref_table = parseSchemaPath(state_);

            // Optional column list
            if (match(TokenType::LEFT_PAREN)) {
                do {
                    constraint.ref_columns.push_back(expectIdentifier("Expected column name"));
                } while (match(TokenType::COMMA));
                expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");
            }

            // ON DELETE / ON UPDATE (DELETE and UPDATE are Gatekeeper keywords)
            while (match(TokenType::KW_ON)) {
                if (match(TokenType::KW_DELETE)) {
                    constraint.on_delete = parseForeignKeyAction();
                } else if (match(TokenType::KW_UPDATE)) {
                    constraint.on_update = parseForeignKeyAction();
                }
            }
            found = true;
        } else if (matchContextual("COLLATE")) {
            constraint.type = ConstraintType::COLLATE;
            constraint.collation = expectIdentifier("Expected collation name");
            found = true;
        } else if (matchContextual("GENERATED")) {
            constraint.type = ConstraintType::GENERATED;
            bool by_default = false;
            if (matchContextual("ALWAYS")) {
                constraint.generated_always = true;
            } else if (matchContextual("BY")) {
                expectContextual("DEFAULT", "Expected DEFAULT after BY");
                by_default = true;
            } else {
                errorCode("PRS_0504", "Expected ALWAYS or BY DEFAULT after GENERATED");
            }

            if (!(match(TokenType::KW_AS) || matchContextual("AS"))) {
                errorCode("PRS_0504", "Expected AS in GENERATED clause");
            }

            if (matchContextual("IDENTITY")) {
                constraint.generated_as_identity = true;
            } else {
                if (by_default) {
                    errorCode("PRS_0504", "Generated expression columns require ALWAYS");
                }
                expect(TokenType::LEFT_PAREN, "Expected '(' after AS");
                constraint.generated_expr = parseExpression();
                expect(TokenType::RIGHT_PAREN, "Expected ')' after expression");
                if (matchContextual("STORED")) {
                    constraint.generated_stored = true;
                } else if (matchContextual("VIRTUAL")) {
                    errorCode("PRS_0504", "VIRTUAL generated columns are not supported");
                } else {
                    errorCode("PRS_0504", "Expected STORED after generated expression");
                }
            }
            found = true;
        }

        if (!found) {
            break;
        }

        bool allow_deferrable =
            (constraint.type == ConstraintType::PRIMARY_KEY) ||
            (constraint.type == ConstraintType::UNIQUE) ||
            (constraint.type == ConstraintType::REFERENCES);
        parseDeferrabilityClause(allow_deferrable,
                                 constraint.deferrable,
                                 constraint.not_deferrable,
                                 constraint.initially_deferred,
                                 constraint.initially_immediate);

        constraint.span = makeSpan(currentLocation());
        constraints.push_back(constraint);
    }

    return constraints;
}

std::vector<DomainConstraint> Parser::parseDomainConstraints() {
    std::vector<DomainConstraint> constraints;

    while (true) {
        StringPool::StringId constraint_name = StringPool::INVALID_ID;
        SourceLocation constraint_start = currentLocation();
        if (matchContextual("CONSTRAINT")) {
            constraint_name = expectIdentifier("Expected constraint name");
        }

        DomainConstraint constraint;
        constraint.name = constraint_name;
        bool found = false;

        if (match(TokenType::KW_NOT)) {
            expect(TokenType::KW_NULL, "Expected NULL after NOT");
            constraint.type = DomainConstraintType::NOT_NULL;
            found = true;
        } else if (match(TokenType::KW_NULL)) {
            constraint.type = DomainConstraintType::NULL_ALLOWED;
            found = true;
        } else if (matchContextual("CHECK")) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after CHECK");
            Expression* expr = parseExpression();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after CHECK expression");
            constraint.type = DomainConstraintType::CHECK;
            constraint.expression = extractExpressionText(expr);
            found = true;
        } else if (match(TokenType::KW_DEFAULT)) {
            Expression* expr = parseExpression();
            constraint.type = DomainConstraintType::DEFAULT;
            constraint.expression = extractExpressionText(expr);
            found = true;
        }

        if (!found) {
            if (constraint_name != StringPool::INVALID_ID) {
                error("Expected domain constraint after CONSTRAINT name");
            }
            break;
        }

        constraint.span = makeSpan(constraint_start);
        constraints.push_back(std::move(constraint));
    }

    return constraints;
}

void Parser::parseDomainIntegrityBlock(CreateDomainStmt* stmt) {
    if (!stmt) {
        return;
    }

    stmt->has_integrity = true;
    expect(TokenType::LEFT_PAREN, "Expected '(' after WITH INTEGRITY");

    auto parse_bool = [&]() -> bool {
        if (match(TokenType::KW_TRUE)) {
            return true;
        }
        if (match(TokenType::KW_FALSE)) {
            return false;
        }
        error("Expected TRUE or FALSE");
        return false;
    };

    auto parse_value_string = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            return std::string(stringPool().get(id));
        }
        if (isIdentifier()) {
            auto id = currentIdentifier();
            return std::string(stringPool().get(id));
        }
        if (isGatekeeperKeyword(current().type)) {
            std::string text(state_.getTokenText(current()));
            advance();
            return text;
        }
        error("Expected identifier or string literal");
        return {};
    };

    auto to_upper = [](std::string_view input) {
        std::string out;
        out.reserve(input.size());
        for (char c : input) {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
        return out;
    };

    while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
        if (matchContextual("UNIQUENESS")) {
            expect(TokenType::EQUAL, "Expected '=' after UNIQUENESS");
            stmt->integrity.has_uniqueness = true;
            stmt->integrity.uniqueness = parse_bool();
        } else if (matchContextual("NORMALIZATION_FUNCTION")) {
            expect(TokenType::EQUAL, "Expected '=' after NORMALIZATION_FUNCTION");
            stmt->integrity.normalization_enabled = true;
            stmt->integrity.normalization_function = parse_value_string();
        } else if (matchContextual("NORMALIZATION")) {
            expect(TokenType::EQUAL, "Expected '=' after NORMALIZATION");
            std::string value = parse_value_string();
            std::string normalized = to_upper(value);
            if (normalized == "NONE") {
                stmt->integrity.normalization_enabled = false;
                stmt->integrity.normalization_function.clear();
            } else {
                stmt->integrity.normalization_enabled = true;
                stmt->integrity.normalization_function = value;
            }
        } else {
            error("Unknown WITH INTEGRITY option");
        }

        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after WITH INTEGRITY options");
}

void Parser::parseDomainSecurityBlock(CreateDomainStmt* stmt) {
    if (!stmt) {
        return;
    }

    stmt->has_security = true;
    expect(TokenType::LEFT_PAREN, "Expected '(' after WITH SECURITY");

    auto parse_bool = [&]() -> bool {
        if (match(TokenType::KW_TRUE)) {
            return true;
        }
        if (match(TokenType::KW_FALSE)) {
            return false;
        }
        error("Expected TRUE or FALSE");
        return false;
    };

    auto parse_value_string = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            return std::string(stringPool().get(id));
        }
        if (isIdentifier()) {
            auto id = currentIdentifier();
            return std::string(stringPool().get(id));
        }
        if (isGatekeeperKeyword(current().type)) {
            std::string text(state_.getTokenText(current()));
            advance();
            return text;
        }
        error("Expected identifier or string literal");
        return {};
    };

    while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
        if (matchContextual("MASKING")) {
            expect(TokenType::EQUAL, "Expected '=' after MASKING");
            stmt->security.has_masking = true;
            stmt->security.masking = parse_value_string();
        } else if (matchContextual("MASK_PATTERN")) {
            expect(TokenType::EQUAL, "Expected '=' after MASK_PATTERN");
            stmt->security.has_mask_pattern = true;
            stmt->security.mask_pattern = parse_value_string();
        } else if (matchContextual("ENCRYPTION")) {
            expect(TokenType::EQUAL, "Expected '=' after ENCRYPTION");
            stmt->security.has_encryption = true;
            stmt->security.encryption = parse_value_string();
        } else if (matchContextual("AUDIT_ACCESS")) {
            expect(TokenType::EQUAL, "Expected '=' after AUDIT_ACCESS");
            stmt->security.has_audit_access = true;
            stmt->security.audit_access = parse_bool();
        } else if (matchContextual("REQUIRE_PRIVILEGE")) {
            expect(TokenType::EQUAL, "Expected '=' after REQUIRE_PRIVILEGE");
            stmt->security.has_required_privilege = true;
            stmt->security.required_privilege = parse_value_string();
        } else if (matchContextual("REQUIRE")) {
            expectContextual("PRIVILEGE", "Expected PRIVILEGE after REQUIRE");
            expect(TokenType::EQUAL, "Expected '=' after REQUIRE PRIVILEGE");
            stmt->security.has_required_privilege = true;
            stmt->security.required_privilege = parse_value_string();
        } else {
            error("Unknown WITH SECURITY option");
        }

        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after WITH SECURITY options");
}

void Parser::parseDomainValidationBlock(CreateDomainStmt* stmt) {
    if (!stmt) {
        return;
    }

    stmt->has_validation = true;
    expect(TokenType::LEFT_PAREN, "Expected '(' after WITH VALIDATION");

    auto parse_value_string = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            return std::string(stringPool().get(id));
        }
        if (isIdentifier()) {
            auto id = currentIdentifier();
            return std::string(stringPool().get(id));
        }
        if (isGatekeeperKeyword(current().type)) {
            std::string text(state_.getTokenText(current()));
            advance();
            return text;
        }
        error("Expected identifier or string literal");
        return {};
    };

    while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
        if (matchContextual("FUNCTION")) {
            expect(TokenType::EQUAL, "Expected '=' after FUNCTION");
            stmt->validation.has_function = true;
            stmt->validation.function = parse_value_string();
        } else if (matchContextual("ERROR_MESSAGE")) {
            expect(TokenType::EQUAL, "Expected '=' after ERROR_MESSAGE");
            stmt->validation.has_error_message = true;
            stmt->validation.error_message = parse_value_string();
        } else {
            error("Unknown WITH VALIDATION option");
        }

        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after WITH VALIDATION options");
}

void Parser::parseDomainQualityBlock(CreateDomainStmt* stmt) {
    if (!stmt) {
        return;
    }

    stmt->has_quality = true;
    expect(TokenType::LEFT_PAREN, "Expected '(' after WITH QUALITY");

    auto parse_value_string = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            return std::string(stringPool().get(id));
        }
        if (isIdentifier()) {
            auto id = currentIdentifier();
            return std::string(stringPool().get(id));
        }
        if (isGatekeeperKeyword(current().type)) {
            std::string text(state_.getTokenText(current()));
            advance();
            return text;
        }
        error("Expected identifier or string literal");
        return {};
    };

    while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
        if (matchContextual("PARSE_FUNCTION")) {
            expect(TokenType::EQUAL, "Expected '=' after PARSE_FUNCTION");
            stmt->quality.has_parse_function = true;
            stmt->quality.parse_function = parse_value_string();
        } else if (matchContextual("STANDARDIZE_FUNCTION")) {
            expect(TokenType::EQUAL, "Expected '=' after STANDARDIZE_FUNCTION");
            stmt->quality.has_standardize_function = true;
            stmt->quality.standardize_function = parse_value_string();
        } else if (matchContextual("ENRICH_FUNCTION")) {
            expect(TokenType::EQUAL, "Expected '=' after ENRICH_FUNCTION");
            stmt->quality.has_enrich_function = true;
            stmt->quality.enrich_function = parse_value_string();
        } else {
            error("Unknown WITH QUALITY option");
        }

        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after WITH QUALITY options");
}

void Parser::parseDomainOptionsBlock(CreateDomainStmt* stmt) {
    if (!stmt) {
        return;
    }

    expect(TokenType::LEFT_PAREN, "Expected '(' after WITH OPTIONS");

    auto parse_bool = [&]() -> bool {
        if (match(TokenType::KW_TRUE)) {
            return true;
        }
        if (match(TokenType::KW_FALSE)) {
            return false;
        }
        error("Expected TRUE or FALSE");
        return false;
    };

    while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
        if (matchContextual("WRAP")) {
            expect(TokenType::EQUAL, "Expected '=' after WRAP");
            stmt->enum_wrap = parse_bool();
        } else {
            error("Unknown WITH OPTIONS entry for CREATE DOMAIN");
            while (!check(TokenType::COMMA) && !check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
                advance();
            }
        }

        if (!match(TokenType::COMMA)) {
            break;
        }
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after WITH OPTIONS options");
}

std::string Parser::extractExpressionText(Expression* expr) {
    if (!expr) {
        return {};
    }

    std::string_view text = state_.lexer().getTokenText(expr->span);
    size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos) {
        return {};
    }
    size_t end = text.find_last_not_of(" \t\r\n");
    return std::string(text.substr(start, end - start + 1));
}

std::string Parser::captureStatementBody() {
    std::string_view input = state_.lexer().input();
    if (input.empty() || isAtEnd()) {
        return {};
    }

    size_t start = current().span.start.offset;
    size_t end = start;
    bool saw_begin = false;
    int begin_depth = 0;
    Token last = current();

    auto trim = [](std::string_view text) -> std::string {
        size_t first = text.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos) {
            return {};
        }
        size_t last_pos = text.find_last_not_of(" \t\r\n");
        return std::string(text.substr(first, last_pos - first + 1));
    };

    auto is_begin_token = [&]() -> bool {
        return check(TokenType::KW_BEGIN) || checkContextual("BEGIN");
    };
    auto is_end_token = [&]() -> bool {
        return check(TokenType::KW_END) || checkContextual("END");
    };

    while (!isAtEnd()) {
        if (is_begin_token()) {
            saw_begin = true;
            begin_depth++;
        } else if (is_end_token()) {
            if (saw_begin && begin_depth > 0) {
                begin_depth--;
                if (begin_depth == 0) {
                    end = current().span.start.offset + current().span.length;
                    advance();
                    if (end > input.size()) {
                        end = input.size();
                    }
                    return trim(input.substr(start, end - start));
                }
            }
        }

        if (!saw_begin && check(TokenType::SEMICOLON)) {
            end = last.span.start.offset + last.span.length;
            if (end > input.size()) {
                end = input.size();
            }
            return trim(input.substr(start, end - start));
        }

        last = current();
        advance();
    }

    if (end == start) {
        end = last.span.start.offset + last.span.length;
        if (end > input.size()) {
            end = input.size();
        }
    }

    return trim(input.substr(start, end - start));
}

ForeignKeyAction Parser::parseForeignKeyAction() {
    if (matchContextual("CASCADE")) return ForeignKeyAction::CASCADE;
    if (matchContextual("RESTRICT")) return ForeignKeyAction::RESTRICT;
    if (matchContextual("NO")) {
        expectContextual("ACTION", "Expected ACTION after NO");
        return ForeignKeyAction::NO_ACTION;
    }
    if (match(TokenType::KW_SET)) {
        if (match(TokenType::KW_NULL)) return ForeignKeyAction::SET_NULL;
        if (match(TokenType::KW_DEFAULT)) return ForeignKeyAction::SET_DEFAULT;
    }
    return ForeignKeyAction::NO_ACTION;
}

// =============================================================================
// Table Constraints
// =============================================================================

TableConstraint* Parser::parseTableConstraint() {
    SourceLocation start = currentLocation();

    auto* constraint = arena_.create<TableConstraint>();

    // Optional CONSTRAINT name
    if (matchContextual("CONSTRAINT")) {
        constraint->name = expectIdentifier("Expected constraint name");
    }

    // Constraint type
    if (matchContextual("PRIMARY")) {
        expectContextual("KEY", "Expected KEY after PRIMARY");
        parsePrimaryKeyConstraint(constraint);
    } else if (matchContextual("UNIQUE")) {
        parseUniqueConstraint(constraint);
    } else if (matchContextual("FOREIGN")) {
        expectContextual("KEY", "Expected KEY after FOREIGN");
        parseForeignKeyConstraint(constraint);
    } else if (matchContextual("CHECK")) {
        parseCheckConstraint(constraint);
    } else if (matchContextual("EXCLUDE")) {
        parseExcludeConstraint(constraint);
    } else {
        error("Expected constraint type (PRIMARY KEY, UNIQUE, FOREIGN KEY, CHECK, EXCLUDE)");
    }

    bool allow_deferrable =
        (constraint->type == TableConstraintType::PRIMARY_KEY) ||
        (constraint->type == TableConstraintType::UNIQUE) ||
        (constraint->type == TableConstraintType::FOREIGN_KEY) ||
        (constraint->type == TableConstraintType::EXCLUDE);
    parseDeferrabilityClause(allow_deferrable,
                             constraint->deferrable,
                             constraint->not_deferrable,
                             constraint->initially_deferred,
                             constraint->initially_immediate);

    constraint->span = makeSpan(start);
    return constraint;
}

void Parser::parsePrimaryKeyConstraint(TableConstraint* constraint) {
    constraint->type = TableConstraintType::PRIMARY_KEY;

    expect(TokenType::LEFT_PAREN, "Expected '(' after PRIMARY KEY");
    do {
        constraint->columns.push_back(expectIdentifier("Expected column name"));
    } while (match(TokenType::COMMA));
    expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");

    // Optional USING INDEX
    if (checkContextual("USING")) {
        matchContextual("USING");
        expectContextual("INDEX", "Expected INDEX after USING");
        constraint->using_index = true;
        constraint->index_method = expectIdentifier("Expected index method");
    }
}

void Parser::parseUniqueConstraint(TableConstraint* constraint) {
    constraint->type = TableConstraintType::UNIQUE;

    expect(TokenType::LEFT_PAREN, "Expected '(' after UNIQUE");
    do {
        constraint->columns.push_back(expectIdentifier("Expected column name"));
    } while (match(TokenType::COMMA));
    expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");
}

void Parser::parseForeignKeyConstraint(TableConstraint* constraint) {
    constraint->type = TableConstraintType::FOREIGN_KEY;

    expect(TokenType::LEFT_PAREN, "Expected '(' after FOREIGN KEY");
    do {
        constraint->columns.push_back(expectIdentifier("Expected column name"));
    } while (match(TokenType::COMMA));
    expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");

    expectContextual("REFERENCES", "Expected REFERENCES after column list");
    constraint->ref_table = parseSchemaPath(state_);

    // Optional referenced column list
    if (match(TokenType::LEFT_PAREN)) {
        do {
            constraint->ref_columns.push_back(expectIdentifier("Expected column name"));
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");
    }

    // ON DELETE / ON UPDATE (DELETE and UPDATE are Gatekeeper keywords)
    while (match(TokenType::KW_ON)) {
        if (match(TokenType::KW_DELETE)) {
            constraint->on_delete = parseForeignKeyAction();
        } else if (match(TokenType::KW_UPDATE)) {
            constraint->on_update = parseForeignKeyAction();
        }
    }
}

void Parser::parseCheckConstraint(TableConstraint* constraint) {
    constraint->type = TableConstraintType::CHECK;

    expect(TokenType::LEFT_PAREN, "Expected '(' after CHECK");
    constraint->check_expr = parseExpression();
    expect(TokenType::RIGHT_PAREN, "Expected ')' after CHECK expression");
}

void Parser::parseExcludeConstraint(TableConstraint* constraint) {
    constraint->type = TableConstraintType::EXCLUDE;

    if (!(match(TokenType::KW_USING) || matchContextual("USING"))) {
        errorCode("PRS_0504", "Expected USING after EXCLUDE");
    }
    constraint->index_method = expectIdentifier("Expected index method after EXCLUDE USING");

    expect(TokenType::LEFT_PAREN, "Expected '(' after EXCLUDE USING method");
    do {
        constraint->exclude_expressions.push_back(parseExpression());

        if (!(match(TokenType::KW_WITH) || matchContextual("WITH"))) {
            errorCode("PRS_0504", "Expected WITH after EXCLUDE element expression");
        }

        StringPool::StringId op_id = StringPool::INVALID_ID;
        if (isIdentifier()) {
            op_id = currentIdentifier();
        } else {
            std::string text(state_.getTokenText(current()));
            if (text.empty()) {
                errorCode("PRS_0504", "Expected exclusion operator after WITH");
            } else {
                op_id = stringPool().intern(text);
                advance();
            }
        }
        constraint->exclude_operators.push_back(op_id);
    } while (match(TokenType::COMMA));
    expect(TokenType::RIGHT_PAREN, "Expected ')' after EXCLUDE element list");

    if (match(TokenType::KW_WHERE)) {
        expect(TokenType::LEFT_PAREN, "Expected '(' after WHERE in EXCLUDE constraint");
        constraint->exclude_where = parseExpression();
        expect(TokenType::RIGHT_PAREN, "Expected ')' after EXCLUDE WHERE expression");
    }
}

void Parser::parseDeferrabilityClause(bool allow_deferrable,
                                      bool& deferrable,
                                      bool& not_deferrable,
                                      bool& initially_deferred,
                                      bool& initially_immediate) {
    auto nextTokenIsDeferrable = [&]() -> bool {
        if (!check(TokenType::KW_NOT)) {
            return false;
        }
        Token next = state_.lexer().peekToken();
        if (next.type != TokenType::IDENTIFIER || next.is_delimited) {
            return false;
        }
        std::string_view text = stringPool().get(next.value.string_id);
        return caseInsensitiveEquals(text, "DEFERRABLE");
    };

    bool saw_any = false;
    if (matchContextual("DEFERRABLE")) {
        deferrable = true;
        saw_any = true;
    } else if (nextTokenIsDeferrable()) {
        advance();  // NOT
        matchContextual("DEFERRABLE");
        not_deferrable = true;
        saw_any = true;
    }

    if (matchContextual("INITIALLY")) {
        saw_any = true;
        if (matchContextual("DEFERRED")) {
            initially_deferred = true;
        } else if (matchContextual("IMMEDIATE")) {
            initially_immediate = true;
        } else {
            errorCode("PRS_0504", "Expected DEFERRED or IMMEDIATE after INITIALLY");
        }
    }

    if (!saw_any) {
        return;
    }

    if (!allow_deferrable) {
        errorCode("PRS_0504", "Constraint type does not support DEFERRABLE");
        return;
    }

    if (not_deferrable && (initially_deferred || initially_immediate)) {
        errorCode("PRS_0504", "NOT DEFERRABLE cannot use INITIALLY clause");
    }

    if (!deferrable && (initially_deferred || initially_immediate)) {
        errorCode("PRS_0504", "INITIALLY clause requires DEFERRABLE");
    }
}

// =============================================================================
// CREATE INDEX
// =============================================================================

CreateIndexStmt* Parser::parseCreateIndex() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateIndexStmt>();

    // Check for CONCURRENTLY
    if (matchContextual("CONCURRENTLY")) {
        stmt->concurrent = true;
    }

    // Check for IF NOT EXISTS (IF, NOT, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    // Index name (optional for some databases)
    // ON is a Gatekeeper keyword, so check TokenType
    if (isIdentifier() && !check(TokenType::KW_ON)) {
        stmt->index_name = currentIdentifier();
    }

    // ON table_name (ON is a Gatekeeper keyword)
    expect(TokenType::KW_ON, "Expected ON after index name");
    stmt->table_path = parseSchemaPath(state_);

    // Optional USING method (USING is a Gatekeeper keyword)
    if (match(TokenType::KW_USING)) {
        if (!isIdentifier()) {
            error("Expected index type after USING");
        } else {
            std::string method_name = std::string(stringPool().get(current().value.string_id));
            advance();
            while (match(TokenType::MINUS)) {
                if (!isIdentifier()) {
                    error("Expected index type segment after '-'");
                    break;
                }
                method_name.push_back('-');
                method_name.append(stringPool().get(current().value.string_id));
                advance();
            }

            stmt->index_method_name = stringPool().intern(method_name);
            auto parsed = indexTypeFromName(method_name);
            if (!parsed.has_value()) {
                error("Unknown index type");
            } else {
                stmt->index_type = *parsed;
            }
        }
    }

    // Column list
    expect(TokenType::LEFT_PAREN, "Expected '(' after table name");
    do {
        IndexColumn col;

        // Could be column name or expression
        if (isIdentifier()) {
            col.column = currentIdentifier();
        } else if (check(TokenType::LEFT_PAREN)) {
            // Expression index
            col.expr = parseParenExpr();
        }

        // Sort order
        if (matchContextual("ASC")) col.ascending = true;
        else if (matchContextual("DESC")) col.ascending = false;

        // NULLS FIRST/LAST
        if (matchContextual("NULLS")) {
            if (matchContextual("FIRST")) col.nulls_first = true;
            else if (matchContextual("LAST")) col.nulls_last = true;
        }

        stmt->columns.push_back(col);
    } while (match(TokenType::COMMA));
    expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");

    // INCLUDE clause
    if (matchContextual("INCLUDE")) {
        expect(TokenType::LEFT_PAREN, "Expected '(' after INCLUDE");
        do {
            stmt->include_columns.push_back(expectIdentifier("Expected column name"));
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after INCLUDE columns");
    }

    // WHERE clause (partial index)
    if (match(TokenType::KW_WHERE)) {
        stmt->where_clause = parseExpression();
    }

    // TABLESPACE
    if (matchContextual("TABLESPACE")) {
        stmt->tablespace = parseSchemaPath(state_);
        stmt->has_tablespace = true;
    }

    // WITH (index options)
    if (match(TokenType::KW_WITH)) {
        expect(TokenType::LEFT_PAREN, "Expected '(' after WITH");

        auto parse_option_value = [&]() -> StringPool::StringId {
            if (match(TokenType::KW_TRUE)) return stringPool().intern("true");
            if (match(TokenType::KW_FALSE)) return stringPool().intern("false");
            if (check(TokenType::INTEGER_LITERAL)) {
                auto id = stringPool().intern(std::to_string(current().value.int_value));
                advance();
                return id;
            }
            if (check(TokenType::FLOAT_LITERAL)) {
                auto id = stringPool().intern(std::to_string(current().value.float_value));
                advance();
                return id;
            }
            if (check(TokenType::STRING_LITERAL) || isIdentifier()) {
                auto id = current().value.string_id;
                advance();
                return id;
            }
            error("Expected scalar value for index option");
            return StringPool::INVALID_ID;
        };

        while (!check(TokenType::RIGHT_PAREN) &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            if (!isIdentifier()) {
                error("Expected index option name");
                break;
            }

            auto opt_name = stringPool().get(current().value.string_id);
            auto opt_name_id = current().value.string_id;
            advance();
            expect(TokenType::EQUAL, "Expected '=' after index option name");
            auto opt_value_id = parse_option_value();
            if (opt_value_id == StringPool::INVALID_ID) {
                break;
            }

            stmt->option_assignments.push_back({opt_name_id, opt_value_id});
            auto opt_value = stringPool().get(opt_value_id);

            if (caseInsensitiveEquals(opt_name, "BLOOM_FILTER")) {
                stmt->options.bloom_filter_enabled =
                    caseInsensitiveEquals(opt_value, "true") || opt_value == "1";
                stmt->options.bloom_filter_set = true;
            } else if (caseInsensitiveEquals(opt_name, "BLOOM_FPR")) {
                try {
                    stmt->options.bloom_fpr = std::stod(std::string(opt_value));
                    stmt->options.bloom_fpr_set = true;
                } catch (...) {
                    error("Expected numeric value for BLOOM_FPR");
                }
            }

            if (!match(TokenType::COMMA)) {
                break;
            }
        }

        expect(TokenType::RIGHT_PAREN, "Expected ')' after index options");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// CREATE VIEW
// =============================================================================

CreateViewStmt* Parser::parseCreateView(bool or_replace) {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateViewStmt>();
    stmt->or_replace = or_replace;

    // Check for IF NOT EXISTS (IF, NOT, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    // View name
    stmt->view_path = parseSchemaPath(state_);

    // Optional column name list
    if (match(TokenType::LEFT_PAREN)) {
        do {
            stmt->column_names.push_back(expectIdentifier("Expected column name"));
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");
    }

    // Optional WITH (...) options before AS
    if (match(TokenType::KW_WITH) && check(TokenType::LEFT_PAREN)) {
        expect(TokenType::LEFT_PAREN, "Expected '(' after WITH");
        while (!check(TokenType::RIGHT_PAREN) &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            StringPool::StringId opt_name = expectIdentifier("Expected view option name");
            expect(TokenType::EQUAL, "Expected '=' after view option name");

            bool bool_value = false;
            bool parsed_bool = false;
            if (match(TokenType::KW_TRUE)) {
                bool_value = true;
                parsed_bool = true;
            } else if (match(TokenType::KW_FALSE)) {
                bool_value = false;
                parsed_bool = true;
            } else if (check(TokenType::INTEGER_LITERAL)) {
                bool_value = current().value.int_value != 0;
                parsed_bool = true;
                advance();
            } else if (isIdentifier()) {
                std::string value = std::string(stringPool().get(current().value.string_id));
                std::string upper;
                upper.reserve(value.size());
                for (char c : value) {
                    upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
                }
                if (upper == "TRUE" || upper == "ON" || upper == "YES") {
                    bool_value = true;
                    parsed_bool = true;
                } else if (upper == "FALSE" || upper == "OFF" || upper == "NO") {
                    bool_value = false;
                    parsed_bool = true;
                }
                advance();
            }

            if (!parsed_bool) {
                error("Expected boolean view option value");
            } else {
                std::string_view opt_name_view = stringPool().get(opt_name);
                if (caseInsensitiveEquals(opt_name_view, "MATERIALIZED")) {
                    stmt->materialized = bool_value;
                }
            }

            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after view options");
    }

    // AS SELECT ...
    expect(TokenType::KW_AS, "Expected AS before SELECT");

    // Parse the SELECT statement
    if (match(TokenType::KW_SELECT)) {
        stmt->query = parseSelect();
    } else {
        error("Expected SELECT after AS in CREATE VIEW");
    }

    // WITH CHECK OPTION
    if (match(TokenType::KW_WITH)) {
        if (matchContextual("CHECK")) {
            expectContextual("OPTION", "Expected OPTION after CHECK");
            stmt->with_check_option = true;
        }
        if (matchContextual("LOCAL")) {
            stmt->check_option_local = true;
        } else if (matchContextual("CASCADED")) {
            stmt->check_option_local = false;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// CREATE SEQUENCE
// =============================================================================

CreateSequenceStmt* Parser::parseCreateSequence() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateSequenceStmt>();

    // Check for IF NOT EXISTS (IF, NOT, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    // Sequence name
    stmt->sequence_path = parseSchemaPath(state_);

    // Sequence options
    while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
        // START is a Gatekeeper keyword
        if (match(TokenType::KW_START)) {
            match(TokenType::KW_WITH);  // WITH is also a Gatekeeper keyword
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->start_with = current().value.int_value;
                advance();
            }
        } else if (matchContextual("INCREMENT")) {
            matchContextual("BY");
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->increment_by = current().value.int_value;
                advance();
            }
        } else if (matchContextual("MINVALUE")) {
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->min_value = current().value.int_value;
                advance();
            }
        } else if (matchContextual("MAXVALUE")) {
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->max_value = current().value.int_value;
                advance();
            }
        } else if (checkContextual("NO")) {
            matchContextual("NO");
            if (matchContextual("MINVALUE")) stmt->no_min_value = true;
            else if (matchContextual("MAXVALUE")) stmt->no_max_value = true;
            else if (matchContextual("CYCLE")) stmt->cycle = false;
        } else if (matchContextual("CACHE")) {
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->cache = current().value.int_value;
                advance();
            }
        } else if (matchContextual("CYCLE")) {
            stmt->cycle = true;
        } else if (matchContextual("OWNED")) {
            expectContextual("BY", "Expected BY after OWNED");
            stmt->owned_by_table = parseSchemaPath(state_);
            expect(TokenType::DOT, "Expected '.' before column name");
            stmt->owned_by_column = expectIdentifier("Expected column name");
            stmt->has_owned_by = true;
        } else {
            break;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// CREATE SCHEMA
// =============================================================================

CreateSchemaStmt* Parser::parseCreateSchema() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateSchemaStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    if (!checkContextual("AUTHORIZATION")) {
        stmt->schema_path = parseSchemaPath(state_);
    }

    if (matchContextual("AUTHORIZATION")) {
        stmt->has_owner = true;
        stmt->owner = expectIdentifier("Expected owner name");
        if (stmt->schema_path.isEmpty()) {
            SchemaPath path;
            path.components.push_back(stmt->owner);
            stmt->schema_path = path;
        }
    }

    if (stmt->schema_path.isEmpty()) {
        error("Expected schema name");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// CREATE DATABASE
// =============================================================================

CreateDatabaseStmt* Parser::parseCreateDatabase() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateDatabaseStmt>();
    // Spec: docs/specifications/ddl/DDL_DATABASES.md

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    auto parse_string_or_identifier = [&]() -> std::string {
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            return std::string(stringPool().get(id));
        }
        if (isIdentifier()) {
            auto id = currentIdentifier();
            return std::string(stringPool().get(id));
        }
        return {};
    };

    auto parse_option_value = [&](bool* ok) -> std::string {
        *ok = true;
        bool negate = false;
        bool saw_sign = false;
        if (match(TokenType::MINUS)) {
            negate = true;
            saw_sign = true;
        } else {
            if (match(TokenType::PLUS)) {
                saw_sign = true;
            }
        }

        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            if (saw_sign) {
                *ok = false;
                return {};
            }
            return std::string(stringPool().get(id));
        }
        if (check(TokenType::INTEGER_LITERAL) || check(TokenType::FLOAT_LITERAL)) {
            std::string text = std::string(state_.getTokenText(current()));
            advance();
            if (negate) {
                return "-" + text;
            }
            return text;
        }
        if (isIdentifier()) {
            if (saw_sign) {
                *ok = false;
                return {};
            }
            auto id = currentIdentifier();
            return std::string(stringPool().get(id));
        }

        *ok = false;
        return {};
    };

    auto parse_alias_id = [&]() -> StringPool::StringId {
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            return id;
        }
        if (isIdentifier()) {
            return currentIdentifier();
        }
        error("Expected alias name");
        return StringPool::INVALID_ID;
    };

    if (matchContextual("EMULATED")) {
        if (!requireFeature(kFeatureEngineProfileCreate)) {
            stmt->span = makeSpan(start);
            return stmt;
        }
        std::string dialect = parse_string_or_identifier();
        if (dialect.empty()) {
            error("Expected emulation dialect after EMULATED");
        }
        auto normalize_lower = [](std::string value) {
            for (char& ch : value) {
                ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
            }
            return value;
        };
        dialect = normalize_lower(dialect);
        {
            static const char* kKnownProfiles[] = {
                "mysql",
                "postgresql",
                "firebird",
                "firebirdsql",
                "cassandra",
                "milvus",
                "mongodb",
                "neo4j",
                "redis",
                "mariadb",
                "influxdb",
                "clickhouse",
                "opensearch",
                "duckdb"
            };
            bool known_profile = false;
            for (const char* profile : kKnownProfiles) {
                if (dialect == profile) {
                    known_profile = true;
                    break;
                }
            }
            if (!known_profile) {
                errorCode("PRS_0503", "Feature not enabled for active profile");
            }
        }

        std::string server;
        bool server_set = false;
        auto parse_server = [&]() {
            server = parse_string_or_identifier();
            if (server.empty()) {
                error("Expected server name");
            }
            server_set = true;
        };

        if (match(TokenType::KW_ON)) {
            expectContextual("SERVER", "Expected SERVER after ON");
            parse_server();
        } else if (matchContextual("SERVER")) {
            parse_server();
        }

        std::string source_spec = parse_string_or_identifier();
        if (source_spec.empty()) {
            error("Expected emulated database source specification");
        }

        auto parse_options_block = [&]() {
            if (matchContextual("OPTIONS") || matchContextual("OPTION")) {
                // OPTIONS keyword consumed
            }
            expect(TokenType::LEFT_PAREN, "Expected '(' after WITH");
            while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
                std::string key = parse_string_or_identifier();
                if (key.empty()) {
                    error("Expected option key");
                    break;
                }

                bool has_value = false;
                std::string value;
                if (match(TokenType::EQUAL) || match(TokenType::EQUALS_GREATER)) {
                    value = parse_option_value(&has_value);
                } else {
                    value = parse_option_value(&has_value);
                }

                if (!has_value) {
                    error("Expected option value");
                    break;
                }

                DatabaseOption opt;
                opt.key = stringPool().intern(key);
                opt.value = stringPool().intern(value);
                stmt->options.push_back(opt);

                if (!match(TokenType::COMMA)) {
                    break;
                }
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after options");
        };

        auto parse_alias_list = [&]() {
            bool has_paren = match(TokenType::LEFT_PAREN);
            while (!isAtEnd()) {
                StringPool::StringId alias = parse_alias_id();
                if (alias != StringPool::INVALID_ID) {
                    stmt->aliases.push_back(alias);
                }
                if (match(TokenType::COMMA)) {
                    continue;
                }
                break;
            }
            if (has_paren) {
                expect(TokenType::RIGHT_PAREN, "Expected ')' after alias list");
            }
        };

        while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
            if (!server_set && match(TokenType::KW_ON)) {
                expectContextual("SERVER", "Expected SERVER after ON");
                parse_server();
                continue;
            }
            if (!server_set && matchContextual("SERVER")) {
                parse_server();
                continue;
            }
            if (matchContextual("ALIAS") || matchContextual("ALIASES")) {
                parse_alias_list();
                continue;
            }
            if (match(TokenType::KW_WITH)) {
                parse_options_block();
                continue;
            }
            break;
        }

        auto is_windows_drive = [](std::string_view spec) {
            return spec.size() >= 2 &&
                   std::isalpha(static_cast<unsigned char>(spec[0])) &&
                   spec[1] == ':' &&
                   (spec.size() == 2 || spec[2] == '/' || spec[2] == '\\');
        };

        std::string spec_server;
        std::string spec_path = source_spec;
        if (!is_windows_drive(spec_path)) {
            size_t colon = spec_path.find(':');
            if (colon != std::string::npos) {
                spec_server = spec_path.substr(0, colon);
                spec_path = spec_path.substr(colon + 1);
            }
        }

        if (!spec_server.empty()) {
            if (server_set && scratchbird::core::IdentifierUtils::toUpper(server) !=
                                  scratchbird::core::IdentifierUtils::toUpper(spec_server)) {
                error("Server specified twice with different values");
            }
            server = spec_server;
        }

        if (server.empty()) {
            server = "localhost";
        }
        server = normalize_lower(server);

        auto split_path_components = [&](std::string_view path_in) {
            std::vector<std::string> components;
            std::string path(path_in);
            while (!path.empty() && (path.front() == '/' || path.front() == '\\')) {
                path.erase(path.begin());
            }

            if (is_windows_drive(path)) {
                char drive = static_cast<char>(std::tolower(static_cast<unsigned char>(path[0])));
                components.push_back(std::string(1, drive));
                path.erase(0, 2);
                if (!path.empty() && (path.front() == '/' || path.front() == '\\')) {
                    path.erase(path.begin());
                }
            }

            std::string current;
            for (char ch : path) {
                if (ch == '/' || ch == '\\') {
                    if (!current.empty()) {
                        components.push_back(current);
                        current.clear();
                    }
                } else {
                    current.push_back(ch);
                }
            }
            if (!current.empty()) {
                components.push_back(current);
            }
            return components;
        };

        std::string db_name;
        std::vector<std::string> path_components;
        bool looks_like_path = spec_path.find('/') != std::string::npos ||
                               spec_path.find('\\') != std::string::npos ||
                               is_windows_drive(spec_path);

        if (looks_like_path) {
            auto components = split_path_components(spec_path);
            if (!components.empty()) {
                db_name = components.back();
                components.pop_back();
                path_components = std::move(components);
            }

            size_t dot = db_name.find_last_of('.');
            if (dot != std::string::npos && dot > 0) {
                db_name = db_name.substr(0, dot);
            }
        } else {
            db_name = spec_path;
        }

        if (db_name.empty()) {
            error("Emulated database name is empty");
        }

        SchemaPath path;
        path.type = PathType::ABSOLUTE;
        path.components.push_back(stringPool().intern("remote"));
        path.components.push_back(stringPool().intern("emulation"));
        path.components.push_back(stringPool().intern(dialect));
        path.components.push_back(stringPool().intern(server));
        for (const auto& comp : path_components) {
            if (!comp.empty()) {
                path.components.push_back(stringPool().intern(comp));
            }
        }
        path.components.push_back(stringPool().intern(db_name));
        stmt->database_path = std::move(path);
        if (!source_spec.empty()) {
            stmt->source_spec = stringPool().intern(source_spec);
        }
    } else {
        stmt->database_path = parseSchemaPath(state_);
        if (stmt->database_path.isEmpty()) {
            error("Expected database name");
        }
        std::string spec = schemaPathToString(stmt->database_path, stringPool());
        if (!spec.empty()) {
            stmt->source_spec = stringPool().intern(spec);
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreateDatabaseConnection() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    bool if_not_exists = false;
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        if_not_exists = true;
    }

    StringPool::StringId connection_name = expectIdentifier("Expected database connection name");
    std::string connection = std::string(stringPool().get(connection_name));
    std::string payload = captureStatementBody();
    if (payload.empty()) {
        errorCode("PRS_0504", "CREATE DATABASE CONNECTION requires connection parameters");
    }

    const std::string payload_upper = toUpperAscii(payload);
    const bool has_endpoint = payload_upper.find("HOST") != std::string::npos ||
                              payload_upper.find("ENDPOINT") != std::string::npos ||
                              payload_upper.find("URI") != std::string::npos;
    const bool has_mount = payload_upper.find("MOUNT") != std::string::npos;
    const bool has_auth_mode = payload_upper.find("AUTH_MODE") != std::string::npos ||
                               payload_upper.find("SECURITY") != std::string::npos;
    const bool has_shared_or_named = payload_upper.find("SHARED") != std::string::npos ||
                                     payload_upper.find("NAMED") != std::string::npos;
    const bool has_identity_detail = payload_upper.find("PASSWORD") != std::string::npos ||
                                     payload_upper.find("ROLE") != std::string::npos ||
                                     payload_upper.find("GROUP") != std::string::npos;

    if (!has_endpoint || !has_mount || !has_auth_mode || !has_shared_or_named || !has_identity_detail) {
        errorCode(
            "PRS_0504",
            "CREATE DATABASE CONNECTION requires endpoint, mount, AUTH_MODE SHARED|NAMED, and password/role/group detail");
    }

    if (if_not_exists) {
        if (!payload.empty()) payload.insert(0, ";");
        payload.insert(0, "IF_NOT_EXISTS=1");
    }

    stmt->name = stringPool().intern("external.database_connection.create." + connection);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// CREATE DOMAIN
// =============================================================================

CreateDomainStmt* Parser::parseCreateDomain() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateDomainStmt>();
    // Spec: docs/specifications/DDL_DOMAINS_COMPREHENSIVE.md

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    stmt->domain_path = parseSchemaPath(state_);
    if (stmt->domain_path.isEmpty()) {
        error("Expected domain name");
    } else if (stmt->domain_path.components.size() != 1) {
        errorCode("PRS_0505",
                  "DOMAIN names are global and must not be schema-qualified");
    }

    // Optional AS keyword
    match(TokenType::KW_AS);

    if (matchContextual("RECORD")) {
        stmt->domain_kind = DomainKind::RECORD;
        expect(TokenType::LEFT_PAREN, "Expected '(' after RECORD");
        while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
            SourceLocation field_start = currentLocation();
            DomainRecordField field;
            field.name = expectIdentifier("Expected field name");
            field.type = parseTypeName();

            if (match(TokenType::KW_NOT)) {
                expect(TokenType::KW_NULL, "Expected NULL after NOT");
                field.nullable = false;
            } else if (match(TokenType::KW_NULL)) {
                field.nullable = true;
            }

            if (match(TokenType::KW_DEFAULT)) {
                Expression* expr = parseExpression();
                field.default_value = extractExpressionText(expr);
                field.has_default = true;
            }

            field.span = makeSpan(field_start);
            stmt->record_fields.push_back(std::move(field));

            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after RECORD fields");
    } else if (matchContextual("ENUM")) {
        stmt->domain_kind = DomainKind::ENUM;
        expect(TokenType::LEFT_PAREN, "Expected '(' after ENUM");
        while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
            SourceLocation value_start = currentLocation();
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal for ENUM label");
                break;
            }
            DomainEnumValue value;
            value.label = current().value.string_id;
            advance();
            if (match(TokenType::EQUAL)) {
                if (!check(TokenType::INTEGER_LITERAL)) {
                    error("Expected integer position after '='");
                } else {
                    value.has_position = true;
                    value.position = static_cast<int32_t>(current().value.int_value);
                    advance();
                }
            }
            value.span = makeSpan(value_start);
            stmt->enum_values.push_back(std::move(value));

            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after ENUM values");
    } else if (match(TokenType::KW_SET) || matchContextual("SET")) {
        stmt->domain_kind = DomainKind::SET;
        expectContextual("OF", "Expected OF after SET");
        stmt->set_element_type = parseTypeName();
    } else if (matchContextual("VARIANT")) {
        stmt->domain_kind = DomainKind::VARIANT;
        expect(TokenType::LEFT_PAREN, "Expected '(' after VARIANT");
        while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
            stmt->variant_allowed_types.push_back(parseTypeName());
            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after VARIANT types");
    } else {
        stmt->domain_kind = DomainKind::BASIC;
        stmt->base_type = parseTypeName();
    }

    if (matchContextual("INHERITS")) {
        expect(TokenType::LEFT_PAREN, "Expected '(' after INHERITS");
        stmt->parent_domain_path = parseSchemaPath(state_);
        stmt->has_inherits = true;
        expect(TokenType::RIGHT_PAREN, "Expected ')' after INHERITS");
    }

    auto parse_option_string = [&]() -> std::string {
        expect(TokenType::LEFT_PAREN, "Expected '('");
        std::string out;
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            out = std::string(stringPool().get(id));
            advance();
        } else if (isIdentifier()) {
            auto id = currentIdentifier();
            out = std::string(stringPool().get(id));
            advance();
        } else {
            error("Expected identifier or string literal");
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')'");
        return out;
    };

    while (true) {
        bool parsed_any = false;

        // Domain constraints and options (DEFAULT/NOT NULL/CHECK)
        StringPool::StringId constraint_name = StringPool::INVALID_ID;
        SourceLocation constraint_start = currentLocation();
        if (matchContextual("CONSTRAINT")) {
            constraint_name = expectIdentifier("Expected constraint name");
        }

        DomainConstraint constraint;
        constraint.name = constraint_name;
        bool found_constraint = false;

        if (match(TokenType::KW_NOT)) {
            expect(TokenType::KW_NULL, "Expected NULL after NOT");
            constraint.type = DomainConstraintType::NOT_NULL;
            found_constraint = true;
        } else if (match(TokenType::KW_NULL)) {
            constraint.type = DomainConstraintType::NULL_ALLOWED;
            found_constraint = true;
        } else if (matchContextual("CHECK")) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after CHECK");
            Expression* expr = parseExpression();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after CHECK expression");
            constraint.type = DomainConstraintType::CHECK;
            constraint.expression = extractExpressionText(expr);
            found_constraint = true;
        } else if (match(TokenType::KW_DEFAULT)) {
            Expression* expr = parseExpression();
            constraint.type = DomainConstraintType::DEFAULT;
            constraint.expression = extractExpressionText(expr);
            found_constraint = true;
        }

        if (found_constraint) {
            constraint.span = makeSpan(constraint_start);
            stmt->constraints.push_back(std::move(constraint));
            parsed_any = true;
        } else if (constraint_name != StringPool::INVALID_ID) {
            error("Expected domain constraint after CONSTRAINT name");
            parsed_any = true;
        }

        if (matchContextual("COLLATE")) {
            stmt->has_collation = true;
            stmt->collation_name = std::string(stringPool().get(expectIdentifier("Expected collation name")));
            parsed_any = true;
        }

        if (match(TokenType::KW_WITH)) {
            if (matchContextual("DIALECT")) {
                stmt->dialect_tag = parse_option_string();
                stmt->has_dialect = true;
            } else if (matchContextual("COMPAT")) {
                stmt->compat_name = parse_option_string();
                stmt->has_compat = true;
            } else if (matchContextual("INTEGRITY")) {
                parseDomainIntegrityBlock(stmt);
            } else if (matchContextual("SECURITY")) {
                parseDomainSecurityBlock(stmt);
            } else if (matchContextual("VALIDATION")) {
                parseDomainValidationBlock(stmt);
            } else if (matchContextual("QUALITY")) {
                parseDomainQualityBlock(stmt);
            } else if (matchContextual("OPTIONS")) {
                parseDomainOptionsBlock(stmt);
            } else {
                error("WITH block type not supported for CREATE DOMAIN");
                if (match(TokenType::LEFT_PAREN)) {
                    int depth = 1;
                    while (!isAtEnd() && depth > 0) {
                        if (match(TokenType::LEFT_PAREN)) {
                            depth++;
                        } else if (match(TokenType::RIGHT_PAREN)) {
                            depth--;
                        } else {
                            advance();
                        }
                    }
                }
            }
            parsed_any = true;
        }

        if (!parsed_any) {
            break;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

CreateTablespaceStmt* Parser::parseCreateTablespace() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateTablespaceStmt>();
    stmt->tablespace_name = expectIdentifier("Expected tablespace name");

    if (!matchContextual("LOCATION")) {
        error("Expected LOCATION for CREATE TABLESPACE");
    } else if (check(TokenType::STRING_LITERAL)) {
        stmt->location = std::string(stringPool().get(current().value.string_id));
        advance();
    } else {
        error("Expected string literal after LOCATION");
    }

    while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
        if (matchContextual("AUTOEXTEND")) {
            stmt->has_autoextend = true;
            if (matchContextual("ON") || matchContextual("TRUE")) {
                stmt->autoextend_enabled = true;
            } else if (matchContextual("OFF") || matchContextual("FALSE")) {
                stmt->autoextend_enabled = false;
            } else {
                error("Expected ON or OFF after AUTOEXTEND");
            }
        } else if (matchContextual("AUTOEXTEND_SIZE")) {
            stmt->has_autoextend_size = true;
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->autoextend_size_mb = static_cast<uint32_t>(current().value.int_value);
                advance();
            } else {
                error("Expected integer for AUTOEXTEND_SIZE");
            }
        } else if (matchContextual("MAXSIZE")) {
            stmt->has_maxsize = true;
            if (matchContextual("UNLIMITED")) {
                stmt->maxsize_unlimited = true;
            } else if (check(TokenType::INTEGER_LITERAL)) {
                stmt->maxsize_mb = static_cast<uint32_t>(current().value.int_value);
                advance();
            } else {
                error("Expected integer or UNLIMITED for MAXSIZE");
            }
        } else if (matchContextual("PREALLOC")) {
            stmt->has_prealloc = true;
            if (check(TokenType::INTEGER_LITERAL)) {
                stmt->prealloc_mb = static_cast<uint32_t>(current().value.int_value);
                advance();
            } else {
                error("Expected integer for PREALLOC");
            }
        } else {
            break;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAlterTablespace() {
    SourceLocation start = currentLocation();

    auto tablespace_name = expectIdentifier("Expected tablespace name");

    if (matchContextual("DETACH")) {
        return parseDetachTablespace(tablespace_name);
    }

    if (matchContextual("ATTACH")) {
        return parseAttachTablespace(tablespace_name);
    }

    auto* stmt = arena_.create<AlterTablespaceStmt>();
    stmt->tablespace_name = tablespace_name;

    while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
        if (matchContextual("AUTOEXTEND")) {
            TablespaceAlteration alt;
            alt.action = TablespaceAlterAction::SET_AUTOEXTEND;
            if (match(TokenType::KW_ON) || match(TokenType::KW_TRUE) ||
                matchContextual("ON") || matchContextual("TRUE")) {
                alt.autoextend_enabled = true;
            } else if (match(TokenType::KW_FALSE) || matchContextual("OFF") ||
                       matchContextual("FALSE")) {
                alt.autoextend_enabled = false;
            } else {
                error("Expected ON or OFF after AUTOEXTEND");
            }
            stmt->alterations.push_back(std::move(alt));
        } else if (matchContextual("AUTOEXTEND_SIZE")) {
            TablespaceAlteration alt;
            alt.action = TablespaceAlterAction::SET_AUTOEXTEND_SIZE;
            if (check(TokenType::INTEGER_LITERAL)) {
                alt.size_mb = static_cast<uint32_t>(current().value.int_value);
                advance();
            } else {
                error("Expected integer for AUTOEXTEND_SIZE");
            }
            stmt->alterations.push_back(std::move(alt));
        } else if (matchContextual("MAXSIZE")) {
            TablespaceAlteration alt;
            alt.action = TablespaceAlterAction::SET_MAXSIZE;
            if (matchContextual("UNLIMITED")) {
                alt.size_mb = 0;
            } else if (check(TokenType::INTEGER_LITERAL)) {
                alt.size_mb = static_cast<uint32_t>(current().value.int_value);
                advance();
            } else {
                error("Expected integer or UNLIMITED for MAXSIZE");
            }
            stmt->alterations.push_back(std::move(alt));
        } else if (matchContextual("RENAME")) {
            TablespaceAlteration alt;
            alt.action = TablespaceAlterAction::RENAME_TO;
            expectContextual("TO", "Expected TO after RENAME");
            alt.new_name = expectIdentifier("Expected new tablespace name");
            stmt->alterations.push_back(std::move(alt));
        } else {
            break;
        }
    }

    if (stmt->alterations.empty()) {
        error("Expected ALTER TABLESPACE action");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

DropTablespaceStmt* Parser::parseDropTablespace() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropTablespaceStmt>();
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }
    stmt->tablespace_name = expectIdentifier("Expected tablespace name");
    if (matchContextual("FORCE")) {
        stmt->force = true;
    }
    stmt->span = makeSpan(start);
    return stmt;
}

AttachTablespaceStmt* Parser::parseAttachTablespace(const StringPool::StringId tablespace_name) {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<AttachTablespaceStmt>();
    stmt->tablespace_name = tablespace_name;

    if (matchContextual("LOCATION")) {
        // LOCATION keyword consumed
    }

    if (check(TokenType::STRING_LITERAL)) {
        stmt->location = std::string(stringPool().get(current().value.string_id));
        advance();
    } else {
        error("Expected string literal for ATTACH TABLESPACE location");
    }

    bool options_remaining = true;
    while (options_remaining) {
        if (matchContextual("VALIDATE")) {
            stmt->validate = true;
        } else if (matchContextual("FORCE") || matchContextual("ALLOW_MISMATCH")) {
            stmt->allow_mismatch = true;
            if (!stmt->validate) {
                stmt->validate = true;
            }
        } else {
            options_remaining = false;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

DetachTablespaceStmt* Parser::parseDetachTablespace(const StringPool::StringId tablespace_name) {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DetachTablespaceStmt>();
    stmt->tablespace_name = tablespace_name;
    if (matchContextual("FORCE")) {
        stmt->force = true;
    }
    stmt->span = makeSpan(start);
    return stmt;
}

CreateGroupStmt* Parser::parseCreateGroup() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateGroupStmt>();
    stmt->group_name = expectIdentifier("Expected group name");
    stmt->span = makeSpan(start);
    return stmt;
}

CreatePolicyStmt* Parser::parseCreatePolicy() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreatePolicyStmt>();
    stmt->policy_name = expectIdentifier("Expected policy name");

    expect(TokenType::KW_ON, "Expected ON after policy name");
    stmt->table_path = parseSchemaPath(state_);

    // AS { PERMISSIVE | RESTRICTIVE }
    if (matchContextual("AS")) {
        if (matchContextual("PERMISSIVE")) {
            stmt->is_permissive = true;
        } else if (matchContextual("RESTRICTIVE")) {
            stmt->is_permissive = false;
        } else {
            error("Expected PERMISSIVE or RESTRICTIVE after AS");
        }
    }

    // FOR { ALL | SELECT | INSERT | UPDATE | DELETE }
    if (matchContextual("FOR")) {
        if (matchContextual("ALL")) {
            stmt->policy_type = PolicyType::ALL;
        } else if (match(TokenType::KW_SELECT)) {
            stmt->policy_type = PolicyType::SELECT;
        } else if (match(TokenType::KW_INSERT)) {
            stmt->policy_type = PolicyType::INSERT;
        } else if (match(TokenType::KW_UPDATE)) {
            stmt->policy_type = PolicyType::UPDATE;
        } else if (match(TokenType::KW_DELETE)) {
            stmt->policy_type = PolicyType::DELETE;
        } else {
            error("Expected ALL, SELECT, INSERT, UPDATE, or DELETE after FOR");
        }
    }

    // TO { role_name | PUBLIC } [, ...]
    if (matchContextual("TO")) {
        do {
            if (matchContextual("PUBLIC")) {
                // PUBLIC means all roles - leave roles empty
            } else {
                StringPool::StringId role_name = expectIdentifier("Expected role name");
                stmt->roles.push_back(role_name);
            }
        } while (match(TokenType::COMMA));
    }

    // USING ( expression )
    if (match(TokenType::KW_USING)) {
        if (match(TokenType::LEFT_PAREN)) {
            stmt->using_expr = parseExpression();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after USING expression");
        } else {
            stmt->using_expr = parseExpression();
        }
    }

    // WITH CHECK ( expression )
    if (match(TokenType::KW_WITH)) {
        expectContextual("CHECK", "Expected CHECK after WITH");
        if (match(TokenType::LEFT_PAREN)) {
            stmt->with_check_expr = parseExpression();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after WITH CHECK expression");
        } else {
            stmt->with_check_expr = parseExpression();
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

CreateForeignServerStmt* Parser::parseCreateForeignServer() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateForeignServerStmt>();
    stmt->server_name = expectIdentifier("Expected server name");

    auto parse_option_list = [&]() -> std::vector<OptionPair> {
        std::vector<OptionPair> options;
        if (!expect(TokenType::LEFT_PAREN, "Expected '(' after OPTIONS")) {
            return options;
        }
        while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
            if (!isIdentifier()) {
                error("Expected option name");
                break;
            }
            std::string key = std::string(stringPool().get(current().value.string_id));
            advance();

            std::string value;
            if (check(TokenType::STRING_LITERAL)) {
                value = std::string(stringPool().get(current().value.string_id));
                advance();
            } else if (check(TokenType::INTEGER_LITERAL)) {
                value = std::to_string(current().value.int_value);
                advance();
            } else if (check(TokenType::FLOAT_LITERAL)) {
                value = std::to_string(current().value.float_value);
                advance();
            } else if (isIdentifier()) {
                value = std::string(stringPool().get(current().value.string_id));
                advance();
            } else {
                error("Expected option value");
            }

            options.push_back({key, value});

            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after OPTIONS");
        return options;
    };

    if (matchContextual("TYPE")) {
        stmt->has_server_type = true;
        if (check(TokenType::STRING_LITERAL)) {
            stmt->server_type = std::string(stringPool().get(current().value.string_id));
            advance();
        } else if (isIdentifier()) {
            stmt->server_type = std::string(stringPool().get(current().value.string_id));
            advance();
        } else {
            error("Expected server type");
        }
    }

    if (matchContextual("VERSION")) {
        stmt->has_server_version = true;
        if (check(TokenType::STRING_LITERAL)) {
            stmt->server_version = std::string(stringPool().get(current().value.string_id));
            advance();
        } else if (isIdentifier()) {
            stmt->server_version = std::string(stringPool().get(current().value.string_id));
            advance();
        } else {
            error("Expected server version");
        }
    }

    expectContextual("FOREIGN", "Expected FOREIGN");
    expectContextual("DATA", "Expected DATA");
    expectContextual("WRAPPER", "Expected WRAPPER");
    stmt->fdw_name = expectIdentifier("Expected foreign data wrapper name");

    if (matchContextual("OPTIONS")) {
        stmt->options = parse_option_list();
    }

    stmt->span = makeSpan(start);
    return stmt;
}

CreateForeignDataWrapperStmt* Parser::parseCreateForeignDataWrapper() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateForeignDataWrapperStmt>();
    stmt->wrapper_name = expectIdentifier("Expected foreign data wrapper name");

    auto parse_option_list = [&]() -> std::vector<OptionPair> {
        std::vector<OptionPair> options;
        if (!expect(TokenType::LEFT_PAREN, "Expected '(' after OPTIONS")) {
            return options;
        }
        while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
            if (!isIdentifier()) {
                error("Expected option name");
                break;
            }
            std::string key = std::string(stringPool().get(current().value.string_id));
            advance();

            std::string value;
            if (check(TokenType::STRING_LITERAL)) {
                value = std::string(stringPool().get(current().value.string_id));
                advance();
            } else if (check(TokenType::INTEGER_LITERAL)) {
                value = std::to_string(current().value.int_value);
                advance();
            } else if (check(TokenType::FLOAT_LITERAL)) {
                value = std::to_string(current().value.float_value);
                advance();
            } else if (isIdentifier()) {
                value = std::string(stringPool().get(current().value.string_id));
                advance();
            } else {
                error("Expected option value");
            }

            options.push_back({key, value});

            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after OPTIONS");
        return options;
    };

    while (!isAtEnd()) {
        if (matchContextual("HANDLER")) {
            stmt->has_handler = true;
            stmt->handler_name = expectIdentifier("Expected handler function name");
            continue;
        }
        if (matchContextual("VALIDATOR")) {
            stmt->has_validator = true;
            stmt->validator_name = expectIdentifier("Expected validator function name");
            continue;
        }
        if (matchContextual("NO")) {
            if (matchContextual("HANDLER")) {
                stmt->has_handler = false;
                stmt->handler_name = StringPool::INVALID_ID;
                continue;
            }
            if (matchContextual("VALIDATOR")) {
                stmt->has_validator = false;
                stmt->validator_name = StringPool::INVALID_ID;
                continue;
            }
            error("Expected HANDLER or VALIDATOR after NO");
            continue;
        }
        if (matchContextual("OPTIONS")) {
            stmt->options = parse_option_list();
            continue;
        }
        break;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

CreateForeignTableStmt* Parser::parseCreateForeignTable() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateForeignTableStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    stmt->table_path = parseSchemaPath(state_);
    if (stmt->table_path.isEmpty()) {
        error("Expected foreign table name");
    }

    if (!expect(TokenType::LEFT_PAREN, "Expected '(' after foreign table name")) {
        return stmt;
    }

    auto parse_option_list = [&]() -> std::vector<OptionPair> {
        std::vector<OptionPair> options;
        if (!expect(TokenType::LEFT_PAREN, "Expected '(' after OPTIONS")) {
            return options;
        }
        while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
            if (!isIdentifier()) {
                error("Expected option name");
                break;
            }
            std::string key = std::string(stringPool().get(current().value.string_id));
            advance();

            std::string value;
            if (check(TokenType::STRING_LITERAL)) {
                value = std::string(stringPool().get(current().value.string_id));
                advance();
            } else if (check(TokenType::INTEGER_LITERAL)) {
                value = std::to_string(current().value.int_value);
                advance();
            } else if (check(TokenType::FLOAT_LITERAL)) {
                value = std::to_string(current().value.float_value);
                advance();
            } else if (isIdentifier()) {
                value = std::string(stringPool().get(current().value.string_id));
                advance();
            } else {
                error("Expected option value");
            }

            options.push_back({key, value});

            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after OPTIONS");
        return options;
    };

    auto extract_type_text = [&](const TypeName& type) -> std::string {
        std::string_view text = state_.lexer().getTokenText(type.span);
        size_t start_pos = text.find_first_not_of(" \t\r\n");
        if (start_pos == std::string_view::npos) {
            return {};
        }
        size_t end_pos = text.find_last_not_of(" \t\r\n");
        return std::string(text.substr(start_pos, end_pos - start_pos + 1));
    };

    bool first = true;
    while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
        if (!first) {
            expect(TokenType::COMMA, "Expected ',' between column definitions");
        }
        first = false;

        if (checkContextual("CONSTRAINT") ||
            checkContextual("PRIMARY") ||
            checkContextual("UNIQUE") ||
            checkContextual("FOREIGN") ||
            checkContextual("CHECK")) {
            parseTableConstraint();
            continue;
        }

        ForeignColumnDef col;
        col.name = expectIdentifier("Expected column name");
        col.type = parseTypeName();
        col.type_text = extract_type_text(col.type);

        if (matchContextual("OPTIONS")) {
            col.options = parse_option_list();
        }

        (void)parseColumnConstraints();
        stmt->columns.push_back(std::move(col));
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after column definitions");

    expectContextual("SERVER", "Expected SERVER after column definitions");
    stmt->server_name = expectIdentifier("Expected foreign server name");

    if (matchContextual("OPTIONS")) {
        stmt->options = parse_option_list();
    }

    stmt->span = makeSpan(start);
    return stmt;
}

CreateUserMappingStmt* Parser::parseCreateUserMapping() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateUserMappingStmt>();

    expectContextual("FOR", "Expected FOR after CREATE USER MAPPING");

    if (matchContextual("CURRENT_USER")) {
        stmt->target = UserMappingTarget::CURRENT_USER;
    } else if (matchContextual("CURRENT")) {
        expectContextual("USER", "Expected USER after CURRENT");
        stmt->target = UserMappingTarget::CURRENT_USER;
    } else if (matchContextual("SESSION_USER")) {
        stmt->target = UserMappingTarget::SESSION_USER;
    } else if (matchContextual("SESSION")) {
        expectContextual("USER", "Expected USER after SESSION");
        stmt->target = UserMappingTarget::SESSION_USER;
    } else if (matchContextual("PUBLIC")) {
        stmt->target = UserMappingTarget::PUBLIC_ROLE;
    } else if (matchContextual("USER")) {
        if (isIdentifier()) {
            stmt->target = UserMappingTarget::USER_NAME;
            stmt->user_name = expectIdentifier("Expected user name");
        } else {
            stmt->target = UserMappingTarget::CURRENT_USER;
        }
    } else {
        stmt->target = UserMappingTarget::USER_NAME;
        stmt->user_name = expectIdentifier("Expected user name");
    }

    expectContextual("SERVER", "Expected SERVER after user mapping target");
    stmt->server_name = expectIdentifier("Expected server name");

    if (matchContextual("OPTIONS")) {
        auto parse_option_list = [&]() -> std::vector<OptionPair> {
            std::vector<OptionPair> options;
            if (!expect(TokenType::LEFT_PAREN, "Expected '(' after OPTIONS")) {
                return options;
            }
            while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
                if (!isIdentifier()) {
                    error("Expected option name");
                    break;
                }
                std::string key = std::string(stringPool().get(current().value.string_id));
                advance();

                std::string value;
                if (check(TokenType::STRING_LITERAL)) {
                    value = std::string(stringPool().get(current().value.string_id));
                    advance();
                } else if (check(TokenType::INTEGER_LITERAL)) {
                    value = std::to_string(current().value.int_value);
                    advance();
                } else if (check(TokenType::FLOAT_LITERAL)) {
                    value = std::to_string(current().value.float_value);
                    advance();
                } else if (isIdentifier()) {
                    value = std::string(stringPool().get(current().value.string_id));
                    advance();
                } else {
                    error("Expected option value");
                }

                options.push_back({key, value});

                if (!match(TokenType::COMMA)) {
                    break;
                }
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after OPTIONS");
            return options;
        };

        stmt->options = parse_option_list();
    }

    stmt->span = makeSpan(start);
    return stmt;
}

CreateSynonymStmt* Parser::parseCreateSynonym(bool is_public) {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateSynonymStmt>();
    stmt->is_public = is_public;
    stmt->synonym_path = parseSchemaPath(state_);
    if (stmt->synonym_path.isEmpty()) {
        error("Expected synonym name");
    }

    expectContextual("FOR", "Expected FOR after synonym name");

    if (matchContextual("TABLE")) {
        stmt->target_type = DdlObjectType::TABLE;
    } else if (matchContextual("VIEW")) {
        stmt->target_type = DdlObjectType::VIEW;
    } else if (matchContextual("SEQUENCE")) {
        stmt->target_type = DdlObjectType::SEQUENCE;
    } else if (matchContextual("FUNCTION")) {
        stmt->target_type = DdlObjectType::FUNCTION;
    } else if (matchContextual("PROCEDURE")) {
        stmt->target_type = DdlObjectType::PROCEDURE;
    } else if (matchContextual("DOMAIN")) {
        stmt->target_type = DdlObjectType::DOMAIN;
    } else if (matchContextual("TYPE")) {
        stmt->target_type = DdlObjectType::COMPOSITE_TYPE;
    } else if (matchContextual("PACKAGE")) {
        stmt->target_type = DdlObjectType::PACKAGE;
    } else if (matchContextual("SCHEMA")) {
        stmt->target_type = DdlObjectType::SCHEMA;
    } else if (matchContextual("DATABASE")) {
        stmt->target_type = DdlObjectType::DATABASE;
    } else if (matchContextual("UDR")) {
        stmt->target_type = DdlObjectType::UDR;
    } else if (matchContextual("FOREIGN")) {
        expectContextual("TABLE", "Expected TABLE after FOREIGN");
        stmt->target_type = DdlObjectType::FOREIGN_TABLE;
    } else {
        error("Expected target object type after FOR");
    }

    stmt->target_path = parseSchemaPath(state_);
    if (stmt->target_path.isEmpty()) {
        error("Expected target object name for synonym");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

CreateUdrStmt* Parser::parseCreateUdr() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateUdrStmt>();

    if (matchContextual("FUNCTION")) {
        stmt->udr_type = UdrObjectType::FUNCTION;
    } else if (matchContextual("PROCEDURE")) {
        stmt->udr_type = UdrObjectType::PROCEDURE;
    } else if (matchContextual("TRIGGER")) {
        stmt->udr_type = UdrObjectType::TRIGGER;
    }

    stmt->udr_path = parseSchemaPath(state_);
    if (stmt->udr_path.isEmpty()) {
        error("Expected UDR name");
    }

    expectContextual("AS", "Expected AS after UDR name");
    if (check(TokenType::STRING_LITERAL)) {
        stmt->library_path = std::string(stringPool().get(current().value.string_id));
        advance();
    } else {
        error("Expected library path string literal");
    }

    expectContextual("ENTRY", "Expected ENTRY after library path");
    if (check(TokenType::STRING_LITERAL)) {
        stmt->entry_point = std::string(stringPool().get(current().value.string_id));
        advance();
    } else {
        error("Expected entry point string literal");
    }

    if (matchContextual("SIGNATURE")) {
        if (check(TokenType::STRING_LITERAL)) {
            stmt->signature = std::string(stringPool().get(current().value.string_id));
            stmt->has_signature = true;
            advance();
        } else {
            error("Expected signature string literal");
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

CreateFunctionStmt* Parser::parseCreateFunction(bool or_replace) {
    auto* stmt = arena_.create<CreateFunctionStmt>();
    stmt->or_replace = or_replace;
    stmt->function_path = parseSchemaPath(state_);

    if (match(TokenType::LEFT_PAREN)) {
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                RoutineParam param;
                if (matchContextual("IN")) {
                    param.mode = RoutineParamMode::IN;
                } else if (matchContextual("OUT")) {
                    param.mode = RoutineParamMode::OUT;
                } else if (matchContextual("INOUT")) {
                    param.mode = RoutineParamMode::INOUT;
                }

                param.name = expectIdentifier("Expected parameter name");
                param.type = parseTypeName();
                if (match(TokenType::KW_DEFAULT) || match(TokenType::EQUAL)) {
                    param.default_value = parseExpression();
                    param.has_default = true;
                }
                stmt->params.push_back(std::move(param));
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after function parameters");
    }

    if (!matchContextual("RETURNS")) {
        error("Expected RETURNS in CREATE FUNCTION");
    }
    stmt->return_type = parseTypeName();

    while (matchContextual("DETERMINISTIC")) {
        stmt->deterministic = true;
    }

    if (matchContextual("SQL")) {
        expectContextual("SECURITY", "Expected SECURITY after SQL");
        if (matchContextual("DEFINER")) {
            stmt->sql_security = RoutineSqlSecurity::DEFINER;
        } else if (matchContextual("INVOKER")) {
            stmt->sql_security = RoutineSqlSecurity::INVOKER;
        }
    }

    if (!(match(TokenType::KW_AS) || matchContextual("AS"))) {
        error("Expected AS before function body");
    }
    std::string body = captureStatementBody();
    if (!body.empty()) {
        stmt->body = stringPool().intern(body);
    }

    return stmt;
}

CreateProcedureStmt* Parser::parseCreateProcedure(bool or_replace) {
    auto* stmt = arena_.create<CreateProcedureStmt>();
    stmt->or_replace = or_replace;
    stmt->procedure_path = parseSchemaPath(state_);

    if (match(TokenType::LEFT_PAREN)) {
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                RoutineParam param;
                if (matchContextual("IN")) {
                    param.mode = RoutineParamMode::IN;
                } else if (matchContextual("OUT")) {
                    param.mode = RoutineParamMode::OUT;
                } else if (matchContextual("INOUT")) {
                    param.mode = RoutineParamMode::INOUT;
                }

                param.name = expectIdentifier("Expected parameter name");
                param.type = parseTypeName();
                if (match(TokenType::KW_DEFAULT) || match(TokenType::EQUAL)) {
                    param.default_value = parseExpression();
                    param.has_default = true;
                }
                stmt->params.push_back(std::move(param));
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after procedure parameters");
    }

    if (matchContextual("RETURNS")) {
        expect(TokenType::LEFT_PAREN, "Expected '(' after RETURNS");
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                RoutineParam param;
                param.mode = RoutineParamMode::OUT;
                param.name = expectIdentifier("Expected return parameter name");
                param.type = parseTypeName();
                stmt->params.push_back(std::move(param));
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after RETURNS parameters");
    }

    if (matchContextual("SQL")) {
        expectContextual("SECURITY", "Expected SECURITY after SQL");
        if (matchContextual("DEFINER")) {
            stmt->sql_security = RoutineSqlSecurity::DEFINER;
        } else if (matchContextual("INVOKER")) {
            stmt->sql_security = RoutineSqlSecurity::INVOKER;
        }
    }

    if (!(match(TokenType::KW_AS) || matchContextual("AS"))) {
        error("Expected AS before procedure body");
    }
    std::string body = captureStatementBody();
    if (!body.empty()) {
        stmt->body = stringPool().intern(body);
    }

    return stmt;
}

CreateTriggerStmt* Parser::parseCreateTrigger(bool or_replace) {
    auto* stmt = arena_.create<CreateTriggerStmt>();
    stmt->or_replace = or_replace;
    stmt->trigger_name = expectIdentifier("Expected trigger name");

    bool has_table_path = false;
    if (matchContextual("FOR")) {
        stmt->table_path = parseSchemaPath(state_);
        has_table_path = true;
    }

    if (matchContextual("ACTIVE")) {
        stmt->active = true;
    } else if (matchContextual("INACTIVE")) {
        stmt->active = false;
    }

    bool saw_timing = false;
    if (matchContextual("BEFORE")) {
        stmt->timing = TriggerTiming::BEFORE;
        saw_timing = true;
    } else if (matchContextual("AFTER")) {
        stmt->timing = TriggerTiming::AFTER;
        saw_timing = true;
    } else if (matchContextual("INSTEAD")) {
        expectContextual("OF", "Expected OF after INSTEAD");
        stmt->timing = TriggerTiming::INSTEAD_OF;
        saw_timing = true;
    } else if (!( !has_table_path && (check(TokenType::KW_ON) || checkContextual("ON")) )) {
        error("Expected BEFORE, AFTER, INSTEAD OF, or ON <database_event> in CREATE TRIGGER");
    }

    stmt->event_mask = 0;
    auto add_event = [&](TriggerEvent event) {
        stmt->event_mask |= static_cast<uint8_t>(1u << static_cast<uint8_t>(event));
    };
    auto match_event = [&](TriggerEvent event, TokenType kw, const char* text) -> bool {
        if (match(kw) || matchContextual(text)) {
            add_event(event);
            return true;
        }
        return false;
    };
    auto parse_database_trigger_event = [&]() -> bool {
        if (matchContextual("CONNECT")) {
            add_event(TriggerEvent::CONNECT);
            return true;
        }
        if (matchContextual("DISCONNECT")) {
            add_event(TriggerEvent::DISCONNECT);
            return true;
        }
        if (matchContextual("TRANSACTION")) {
            if (matchContextual("START")) {
                add_event(TriggerEvent::TRANSACTION_START);
            } else if (matchContextual("COMMIT")) {
                add_event(TriggerEvent::TRANSACTION_COMMIT);
            } else if (matchContextual("ROLLBACK")) {
                add_event(TriggerEvent::TRANSACTION_ROLLBACK);
            } else {
                error("Expected START, COMMIT, or ROLLBACK after TRANSACTION");
                return false;
            }
            return true;
        }
        return false;
    };

    if (!has_table_path && (match(TokenType::KW_ON) || matchContextual("ON"))) {
        if (parse_database_trigger_event()) {
            stmt->is_database_trigger = true;
            stmt->timing = TriggerTiming::AFTER;
            stmt->granularity = TriggerGranularity::FOR_EACH_STATEMENT;
            while (match(TokenType::KW_OR) || matchContextual("OR")) {
                if (!parse_database_trigger_event()) {
                    error("Expected database trigger event after OR");
                    break;
                }
            }
        } else {
            stmt->table_path = parseSchemaPath(state_);
            has_table_path = true;
        }
    }

    if (!stmt->is_database_trigger) {
        if (!saw_timing) {
            error("Expected BEFORE, AFTER, or INSTEAD OF for table trigger");
        }
        if (!match_event(TriggerEvent::INSERT, TokenType::KW_INSERT, "INSERT") &&
            !match_event(TriggerEvent::UPDATE, TokenType::KW_UPDATE, "UPDATE") &&
            !match_event(TriggerEvent::DELETE, TokenType::KW_DELETE, "DELETE")) {
            // No-op; handled by event_mask check below.
        }

        if (stmt->event_mask == 0) {
            error("Expected INSERT, UPDATE, or DELETE in CREATE TRIGGER");
        }

        while (match(TokenType::KW_OR) || matchContextual("OR")) {
            if (match_event(TriggerEvent::INSERT, TokenType::KW_INSERT, "INSERT") ||
                match_event(TriggerEvent::UPDATE, TokenType::KW_UPDATE, "UPDATE") ||
                match_event(TriggerEvent::DELETE, TokenType::KW_DELETE, "DELETE")) {
                continue;
            }
            error("Expected trigger event after OR");
            break;
        }

        if (!has_table_path) {
            if (match(TokenType::KW_ON) || matchContextual("ON")) {
                stmt->table_path = parseSchemaPath(state_);
                has_table_path = true;
            } else {
                error("Expected ON <table> or FOR <table> in CREATE TRIGGER");
            }
        }

        if (matchContextual("FOR")) {
            matchContextual("EACH");
            if (matchContextual("ROW")) {
                stmt->granularity = TriggerGranularity::FOR_EACH_ROW;
            } else if (matchContextual("STATEMENT")) {
                stmt->granularity = TriggerGranularity::FOR_EACH_STATEMENT;
            }
        }
    }

    if (matchContextual("SQL")) {
        expectContextual("SECURITY", "Expected SECURITY after SQL");
        stmt->has_sql_security = true;
        if (matchContextual("DEFINER")) {
            stmt->sql_security = RoutineSqlSecurity::DEFINER;
        } else if (matchContextual("INVOKER")) {
            stmt->sql_security = RoutineSqlSecurity::INVOKER;
        } else {
            error("Expected DEFINER or INVOKER after SQL SECURITY");
        }
    }

    if (matchContextual("POSITION")) {
        if (check(TokenType::INTEGER_LITERAL)) {
            advance();
        }
    }

    if (check(TokenType::KW_EXECUTE)) {
        std::string body = captureStatementBody();
        if (!body.empty()) {
            stmt->body = stringPool().intern(body);
        }
        return stmt;
    }

    if (!(match(TokenType::KW_AS) || matchContextual("AS"))) {
        error("Expected AS before trigger body");
    }
    std::string body = captureStatementBody();
    if (!body.empty()) {
        stmt->body = stringPool().intern(body);
    }

    return stmt;
}

CreatePackageStmt* Parser::parseCreatePackage(bool or_replace) {
    auto* stmt = arena_.create<CreatePackageStmt>();
    stmt->or_replace = or_replace;

    if (matchContextual("BODY")) {
        stmt->is_body = true;
    }

    stmt->package_path = parseSchemaPath(state_);
    if (!(match(TokenType::KW_AS) || matchContextual("AS"))) {
        error("Expected AS before package body");
    }
    std::string body = captureStatementBody();
    if (!body.empty()) {
        if (stmt->is_body) {
            stmt->body = stringPool().intern(body);
        } else {
            stmt->header = stringPool().intern(body);
        }
    }

    return stmt;
}

CreateExceptionStmt* Parser::parseCreateException(bool or_replace) {
    auto* stmt = arena_.create<CreateExceptionStmt>();
    stmt->or_replace = or_replace;
    stmt->exception_path = parseSchemaPath(state_);

    if (check(TokenType::STRING_LITERAL)) {
        stmt->message = current().value.string_id;
        advance();
    } else if (check(TokenType::IDENTIFIER)) {
        stmt->message = expectIdentifier("Expected exception message");
    } else {
        error("Expected exception message");
    }

    return stmt;
}

CreateTypeStmt* Parser::parseCreateType(bool /*or_replace*/) {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CreateTypeStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        stmt->if_not_exists = true;
    }

    stmt->type_path = parseSchemaPath(state_);
    if (stmt->type_path.isEmpty()) {
        error("Expected type name");
    }

    if (match(TokenType::KW_AS) || matchContextual("AS")) {
        // Optional AS keyword for type definition.
    }

    auto parse_record_fields = [&]() {
        expect(TokenType::LEFT_PAREN, "Expected '(' after RECORD");
        while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
            SourceLocation field_start = currentLocation();
            DomainRecordField field;
            field.name = expectIdentifier("Expected field name");
            field.type = parseTypeName();
            if (matchContextual("COLLATE")) {
                expectIdentifier("Expected collation name");
            }
            if (match(TokenType::KW_NOT)) {
                expect(TokenType::KW_NULL, "Expected NULL after NOT");
                field.nullable = false;
            } else if (match(TokenType::KW_NULL)) {
                field.nullable = true;
            }
            if (match(TokenType::KW_DEFAULT)) {
                Expression* expr = parseExpression();
                field.default_value = extractExpressionText(expr);
                field.has_default = true;
            }
            field.span = makeSpan(field_start);
            stmt->record_fields.push_back(std::move(field));
            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after RECORD fields");
    };

    auto parse_enum_values = [&]() {
        expect(TokenType::LEFT_PAREN, "Expected '(' after ENUM");
        while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
            SourceLocation value_start = currentLocation();
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal for ENUM label");
                break;
            }
            DomainEnumValue value;
            value.label = current().value.string_id;
            advance();
            if (match(TokenType::EQUAL)) {
                if (!check(TokenType::INTEGER_LITERAL)) {
                    error("Expected integer position after '='");
                } else {
                    value.has_position = true;
                    value.position = static_cast<int32_t>(current().value.int_value);
                    advance();
                }
            }
            value.span = makeSpan(value_start);
            stmt->enum_values.push_back(std::move(value));
            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after ENUM values");
    };

    auto parse_range_options = [&]() {
        stmt->range_options = RangeTypeOptions{};
        expect(TokenType::LEFT_PAREN, "Expected '(' after RANGE");
        while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
            StringPool::StringId key_id = expectIdentifier("Expected RANGE option name");
            std::string key = std::string(stringPool().get(key_id));
            expect(TokenType::EQUAL, "Expected '=' after RANGE option name");
            if (key == "SUBTYPE") {
                stmt->range_options.subtype = parseTypeName();
                stmt->range_options.has_subtype = true;
            } else if (key == "SUBTYPE_COLLATION") {
                stmt->range_options.subtype_collation = std::string(stringPool().get(expectIdentifier("Expected collation name")));
                stmt->range_options.has_subtype_collation = true;
            } else if (key == "SUBTYPE_OPCLASS") {
                stmt->range_options.subtype_opclass = std::string(stringPool().get(expectIdentifier("Expected opclass name")));
                stmt->range_options.has_subtype_opclass = true;
            } else if (key == "CANONICAL") {
                stmt->range_options.canonical = std::string(stringPool().get(expectIdentifier("Expected function name")));
                stmt->range_options.has_canonical = true;
            } else if (key == "SUBTYPE_DIFF") {
                stmt->range_options.subtype_diff = std::string(stringPool().get(expectIdentifier("Expected function name")));
                stmt->range_options.has_subtype_diff = true;
            } else if (key == "MULTIRANGE") {
                if (matchContextual("TRUE")) {
                    stmt->range_options.multirange = true;
                } else if (matchContextual("FALSE")) {
                    stmt->range_options.multirange = false;
                } else if (check(TokenType::INTEGER_LITERAL)) {
                    stmt->range_options.multirange = current().value.int_value != 0;
                    advance();
                } else {
                    error("Expected TRUE/FALSE for MULTIRANGE");
                }
                stmt->range_options.has_multirange = true;
            } else {
                error("Unsupported RANGE option: " + key);
            }
            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after RANGE options");
    };

    auto parse_base_options = [&]() {
        stmt->base_options = BaseTypeOptions{};
        expect(TokenType::LEFT_PAREN, "Expected '(' after BASE");
        while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
            StringPool::StringId key_id = expectIdentifier("Expected BASE option name");
            std::string key = std::string(stringPool().get(key_id));
            expect(TokenType::EQUAL, "Expected '=' after BASE option name");
            if (key == "STORAGE") {
                stmt->base_options.storage = parseTypeName();
                stmt->base_options.has_storage = true;
            } else if (key == "INPUT") {
                stmt->base_options.input_function = std::string(stringPool().get(expectIdentifier("Expected function name")));
            } else if (key == "OUTPUT") {
                stmt->base_options.output_function = std::string(stringPool().get(expectIdentifier("Expected function name")));
            } else if (key == "RECEIVE") {
                stmt->base_options.receive_function = std::string(stringPool().get(expectIdentifier("Expected function name")));
                stmt->base_options.has_receive = true;
            } else if (key == "SEND") {
                stmt->base_options.send_function = std::string(stringPool().get(expectIdentifier("Expected function name")));
                stmt->base_options.has_send = true;
            } else if (key == "TYPMOD_IN") {
                stmt->base_options.typmod_in_function = std::string(stringPool().get(expectIdentifier("Expected function name")));
                stmt->base_options.has_typmod_in = true;
            } else if (key == "TYPMOD_OUT") {
                stmt->base_options.typmod_out_function = std::string(stringPool().get(expectIdentifier("Expected function name")));
                stmt->base_options.has_typmod_out = true;
            } else if (key == "ANALYZE") {
                stmt->base_options.analyze_function = std::string(stringPool().get(expectIdentifier("Expected function name")));
                stmt->base_options.has_analyze = true;
            } else if (key == "ALIGNMENT") {
                if (matchContextual("CHAR")) {
                    stmt->base_options.alignment = BaseTypeAlignment::CHAR;
                } else if (matchContextual("SHORT")) {
                    stmt->base_options.alignment = BaseTypeAlignment::SHORT;
                } else if (matchContextual("INT")) {
                    stmt->base_options.alignment = BaseTypeAlignment::INT;
                } else if (matchContextual("DOUBLE")) {
                    stmt->base_options.alignment = BaseTypeAlignment::DOUBLE;
                } else {
                    error("Expected CHAR/SHORT/INT/DOUBLE for ALIGNMENT");
                }
                stmt->base_options.has_alignment = true;
            } else if (key == "STORAGE_MODE") {
                if (matchContextual("PLAIN")) {
                    stmt->base_options.storage_mode = BaseTypeStorageMode::PLAIN;
                } else if (matchContextual("EXTERNAL")) {
                    stmt->base_options.storage_mode = BaseTypeStorageMode::EXTERNAL;
                } else if (matchContextual("EXTENDED")) {
                    stmt->base_options.storage_mode = BaseTypeStorageMode::EXTENDED;
                } else if (matchContextual("MAIN")) {
                    stmt->base_options.storage_mode = BaseTypeStorageMode::MAIN;
                } else {
                    error("Expected PLAIN/EXTERNAL/EXTENDED/MAIN for STORAGE_MODE");
                }
                stmt->base_options.has_storage_mode = true;
            } else if (key == "CATEGORY") {
                if (check(TokenType::STRING_LITERAL)) {
                    auto text = std::string(stringPool().get(current().value.string_id));
                    advance();
                    if (!text.empty()) {
                        stmt->base_options.category = text[0];
                        stmt->base_options.has_category = true;
                    }
                } else {
                    error("Expected string literal for CATEGORY");
                }
            } else if (key == "PREFERRED") {
                if (matchContextual("TRUE")) {
                    stmt->base_options.preferred = true;
                } else if (matchContextual("FALSE")) {
                    stmt->base_options.preferred = false;
                } else if (check(TokenType::INTEGER_LITERAL)) {
                    stmt->base_options.preferred = current().value.int_value != 0;
                    advance();
                } else {
                    error("Expected TRUE/FALSE for PREFERRED");
                }
                stmt->base_options.has_preferred = true;
            } else {
                error("Unsupported BASE option: " + key);
            }

            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after BASE options");
    };

    if (matchContextual("ENUM")) {
        stmt->type_kind = TypeKind::ENUM;
        parse_enum_values();
    } else if (matchContextual("RECORD")) {
        stmt->type_kind = TypeKind::RECORD;
        parse_record_fields();
    } else if (check(TokenType::LEFT_PAREN)) {
        stmt->type_kind = TypeKind::RECORD;
        parse_record_fields();
    } else if (matchContextual("RANGE")) {
        stmt->type_kind = TypeKind::RANGE;
        parse_range_options();
    } else if (matchContextual("BASE")) {
        stmt->type_kind = TypeKind::BASE;
        parse_base_options();
    } else if (matchContextual("SHELL")) {
        stmt->type_kind = TypeKind::SHELL;
        stmt->is_shell = true;
    } else {
        error("Expected ENUM, RECORD, RANGE, BASE, or SHELL in CREATE TYPE");
    }

    while (match(TokenType::KW_WITH)) {
        if (matchContextual("DIALECT")) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after WITH DIALECT");
            if (check(TokenType::STRING_LITERAL)) {
                stmt->dialect_tag = std::string(stringPool().get(current().value.string_id));
                advance();
            } else if (isIdentifier()) {
                stmt->dialect_tag = std::string(stringPool().get(currentIdentifier()));
                advance();
            } else {
                error("Expected dialect name");
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after DIALECT");
            stmt->has_dialect = true;
        } else if (matchContextual("COMPAT")) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after WITH COMPAT");
            if (check(TokenType::STRING_LITERAL)) {
                stmt->compat_name = std::string(stringPool().get(current().value.string_id));
                advance();
            } else if (isIdentifier()) {
                stmt->compat_name = std::string(stringPool().get(currentIdentifier()));
                advance();
            } else {
                error("Expected compat name");
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after COMPAT");
            stmt->has_compat = true;
        } else {
            error("Expected DIALECT or COMPAT after WITH");
        }
    }

    if (matchContextual("COMMENT")) {
        if (!check(TokenType::STRING_LITERAL)) {
            error("Expected string literal after COMMENT");
        } else {
            stmt->comment = std::string(stringPool().get(current().value.string_id));
            stmt->has_comment = true;
            advance();
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// ALTER Statements
// =============================================================================

Statement* Parser::parseAlterMeasurement() {
    SourceLocation start = currentLocation();

    auto path = parseSchemaPath(state_);
    if (path.isEmpty()) {
        error("Expected measurement name");
    }

    if (!matchContextual("RETENTION")) {
        errorCode("PRS_0505", "ALTER MEASUREMENT requires RETENTION");
    }

    std::string duration;
    if (check(TokenType::STRING_LITERAL)) {
        duration = std::string(stringPool().get(current().value.string_id));
        advance();
    } else if (check(TokenType::INTEGER_LITERAL)) {
        duration = std::to_string(current().value.int_value);
        advance();
        if (isIdentifier()) {
            duration.push_back(' ');
            duration.append(stringPool().get(current().value.string_id));
            advance();
        }
    } else {
        errorCode("PRS_0504", "Expected duration literal after RETENTION");
    }

    auto* stmt = arena_.create<AlterSystemStmt>();
    stmt->name = stringPool().intern("measurement.retention." + schemaPathToString(path, stringPool()));
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(duration);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAlterSchedule() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterJobStmt>();
    stmt->job_name = expectIdentifier("Expected schedule name");

    if (!(match(TokenType::KW_SET) || matchContextual("SET"))) {
        errorCode("PRS_0507", "Expected SET after ALTER SCHEDULE");
    }

    auto trim = [](std::string_view s) -> std::string_view {
        size_t b = 0;
        while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) {
            ++b;
        }
        size_t e = s.size();
        while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
            --e;
        }
        return s.substr(b, e - b);
    };

    auto parse_string_lit = [&](const char* context) -> std::string {
        if (!check(TokenType::STRING_LITERAL)) {
            errorCode("PRS_0507", std::string("Expected string literal for ") + context);
            return {};
        }
        auto value = std::string(stringPool().get(current().value.string_id));
        advance();
        return value;
    };

    auto parse_rrule = [&](const std::string& raw_in, std::string& canonical_out) -> bool {
        std::set<std::string> seen_keys;
        std::vector<std::pair<std::string, std::string>> kv_pairs;

        std::string raw(raw_in);
        size_t pos = 0;
        while (pos < raw.size()) {
            size_t next = raw.find(';', pos);
            std::string token = std::string(trim(std::string_view(raw).substr(
                pos, next == std::string::npos ? std::string::npos : next - pos)));
            if (token.empty()) {
                errorCode("PRS_0507", "Invalid RRULE token");
                return false;
            }
            size_t eq = token.find('=');
            if (eq == std::string::npos || eq == 0 || eq + 1 >= token.size()) {
                errorCode("PRS_0507", "Invalid RRULE key/value contract");
                return false;
            }
            std::string key = token.substr(0, eq);
            std::string value = token.substr(eq + 1);
            key = std::string(trim(key));
            value = std::string(trim(value));
            for (char& c : key) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }

            static const char* kAllowed[] = {
                "FREQ", "INTERVAL", "COUNT", "UNTIL", "BYSECOND", "BYMINUTE", "BYHOUR",
                "BYDAY", "BYMONTHDAY", "BYYEARDAY", "BYWEEKNO", "BYMONTH", "BYSETPOS", "WKST"
            };
            bool allowed = false;
            for (const char* candidate : kAllowed) {
                if (key == candidate) {
                    allowed = true;
                    break;
                }
            }
            if (!allowed) {
                errorCode("PRS_0507", "Invalid RRULE key");
                return false;
            }
            if (!seen_keys.insert(key).second) {
                errorCode("PRS_0507", "Duplicate RRULE key");
                return false;
            }
            kv_pairs.push_back({std::move(key), std::move(value)});

            if (next == std::string::npos) {
                break;
            }
            pos = next + 1;
        }

        if (seen_keys.find("FREQ") == seen_keys.end()) {
            errorCode("PRS_0507", "RRULE requires FREQ");
            return false;
        }

        std::sort(kv_pairs.begin(), kv_pairs.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

        std::string canonical;
        for (size_t i = 0; i < kv_pairs.size(); ++i) {
            if (i != 0) {
                canonical.push_back(';');
            }
            canonical.append(kv_pairs[i].first);
            canonical.push_back('=');
            canonical.append(kv_pairs[i].second);
        }
        canonical_out = std::move(canonical);
        return true;
    };

    auto parse_local_ts_list = [&](const char* context, std::vector<std::string>& out) -> bool {
        if (!expect(TokenType::LEFT_PAREN, std::string("Expected '(' after ").append(context))) {
            return false;
        }
        if (check(TokenType::RIGHT_PAREN)) {
            errorCode("PRS_0507", std::string(context) + " list must not be empty");
            advance();
            return false;
        }
        while (!check(TokenType::RIGHT_PAREN) &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            out.push_back(parse_string_lit(context));
            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, std::string("Expected ')' after ").append(context));
        return !out.empty();
    };

    std::string recurrence;
    if (matchContextual("RRULE")) {
        std::string raw_rrule = parse_string_lit("RRULE");
        std::string canonical_rrule;
        if (parse_rrule(raw_rrule, canonical_rrule)) {
            recurrence = "RRULE " + canonical_rrule;
        }
    } else if (matchContextual("RRULE_SET")) {
        if (!expect(TokenType::LEFT_PAREN, "Expected '(' after RRULE_SET")) {
            stmt->span = makeSpan(start);
            return stmt;
        }
        std::set<std::string> unique_rules;
        bool first = true;
        while (!check(TokenType::RIGHT_PAREN) &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            std::string raw_rrule = parse_string_lit("RRULE_SET item");
            std::string canonical_rrule;
            if (parse_rrule(raw_rrule, canonical_rrule)) {
                unique_rules.insert(canonical_rrule);
            }
            first = false;
            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after RRULE_SET");
        if (unique_rules.size() < 2) {
            errorCode("PRS_0507", "RRULE_SET requires at least two unique RRULE members");
        }
        recurrence = "RRULE_SET(";
        first = true;
        for (const auto& member : unique_rules) {
            if (!first) {
                recurrence.push_back(',');
            }
            recurrence.append(member);
            first = false;
        }
        recurrence.push_back(')');
    } else {
        errorCode("PRS_0507", "Expected RRULE or RRULE_SET");
    }

    if (!matchContextual("DTSTART")) {
        errorCode("PRS_0507", "Missing DTSTART");
    }
    std::string dtstart = parse_string_lit("DTSTART");
    if (!matchContextual("TZ")) {
        errorCode("PRS_0507", "Missing TZ");
    }
    std::string timezone = parse_string_lit("TZ");

    std::vector<std::string> rdate_list;
    std::vector<std::string> exdate_list;
    while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
        if (matchContextual("RDATE")) {
            parse_local_ts_list("RDATE", rdate_list);
            continue;
        }
        if (matchContextual("EXDATE")) {
            parse_local_ts_list("EXDATE", exdate_list);
            continue;
        }
        errorCode("PRS_0508", "Unsupported recurrence-source token");
        break;
    }

    std::string payload = recurrence + " DTSTART=" + dtstart + " TZ=" + timezone;
    if (!rdate_list.empty()) {
        payload.append(" RDATE=(");
        for (size_t i = 0; i < rdate_list.size(); ++i) {
            if (i != 0) payload.push_back(',');
            payload.append(rdate_list[i]);
        }
        payload.push_back(')');
    }
    if (!exdate_list.empty()) {
        payload.append(" EXDATE=(");
        for (size_t i = 0; i < exdate_list.size(); ++i) {
            if (i != 0) payload.push_back(',');
            payload.append(exdate_list[i]);
        }
        payload.push_back(')');
    }

    stmt->has_schedule = true;
    stmt->schedule_kind = JobScheduleKind::CRON;
    stmt->cron_expression = stringPool().intern(payload);
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAlterConnectionRule() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();
    auto match_word = [&](TokenType token, const char* word) {
        return match(token) || matchContextual(word);
    };

    StringPool::StringId rule_name = expectIdentifier("Expected connection rule name");
    std::string rule = std::string(stringPool().get(rule_name));

    if (!(match(TokenType::KW_SET) || matchContextual("SET"))) {
        errorCode("SEC_1235", "ALTER CONNECTION RULE requires SET clause");
    }
    if (!expect(TokenType::LEFT_PAREN, "Expected '(' after SET")) {
        stmt->span = makeSpan(start);
        return stmt;
    }

    std::string_view input = state_.lexer().input();
    size_t block_start = current().span.start.offset;
    size_t block_end = block_start;
    int depth = 1;
    Token last = current();
    bool saw_token = false;
    while (!isAtEnd()) {
        if (check(TokenType::LEFT_PAREN)) {
            ++depth;
        } else if (check(TokenType::RIGHT_PAREN)) {
            --depth;
            if (depth == 0) {
                break;
            }
        }
        last = current();
        saw_token = true;
        advance();
    }
    expect(TokenType::RIGHT_PAREN, "Expected ')' after SET clause");
    block_end = last.span.start.offset + last.span.length;
    if (block_end > input.size()) {
        block_end = input.size();
    }

    if (!matchContextual("EXPECT") ||
        !matchContextual("VERSION") ||
        !check(TokenType::INTEGER_LITERAL)) {
        errorCode("SEC_1237", "ALTER CONNECTION RULE requires EXPECT VERSION <uint64>");
    } else {
        advance();
    }

    stmt->name = stringPool().intern("security.connection_rule.alter." + rule);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(
        saw_token && block_end > block_start ? std::string(input.substr(block_start, block_end - block_start))
                                             : std::string());
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAlterToken() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    StringPool::StringId token_name = expectIdentifier("Expected token name");
    std::string token = std::string(stringPool().get(token_name));

    if (!(match(TokenType::KW_SET) || matchContextual("SET"))) {
        errorCode("PRS_0505", "ALTER TOKEN requires SET clause");
    }

    std::string payload;
    if (expect(TokenType::LEFT_PAREN, "Expected '(' after ALTER TOKEN ... SET")) {
        std::string_view input = state_.lexer().input();
        size_t block_start = current().span.start.offset;
        size_t block_end = block_start;
        int depth = 1;
        bool saw_token = false;
        Token last = current();
        while (!isAtEnd()) {
            if (check(TokenType::LEFT_PAREN)) {
                ++depth;
            } else if (check(TokenType::RIGHT_PAREN)) {
                --depth;
                if (depth == 0) {
                    break;
                }
            }
            last = current();
            saw_token = true;
            advance();
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after ALTER TOKEN SET");
        block_end = last.span.start.offset + last.span.length;
        if (block_end > input.size()) {
            block_end = input.size();
        }
        if (saw_token && block_end > block_start) {
            payload = std::string(input.substr(block_start, block_end - block_start));
        }
    }

    stmt->name = stringPool().intern("security.token.alter." + token);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAlterQuotaProfile() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    StringPool::StringId profile_name = expectIdentifier("Expected quota profile name");
    std::string profile = std::string(stringPool().get(profile_name));

    if (!(match(TokenType::KW_SET) || matchContextual("SET"))) {
        errorCode("PRS_0505", "ALTER QUOTA PROFILE requires SET");
    }

    std::string payload;
    if (expect(TokenType::LEFT_PAREN, "Expected '(' after ALTER QUOTA PROFILE ... SET")) {
        std::string_view input = state_.lexer().input();
        size_t block_start = current().span.start.offset;
        size_t block_end = block_start;
        int depth = 1;
        bool saw_token = false;
        Token last = current();
        while (!isAtEnd()) {
            if (check(TokenType::LEFT_PAREN)) {
                ++depth;
            } else if (check(TokenType::RIGHT_PAREN)) {
                --depth;
                if (depth == 0) {
                    break;
                }
            }
            last = current();
            saw_token = true;
            advance();
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after ALTER QUOTA PROFILE SET");
        block_end = last.span.start.offset + last.span.length;
        if (block_end > input.size()) {
            block_end = input.size();
        }
        if (saw_token && block_end > block_start) {
            payload = std::string(input.substr(block_start, block_end - block_start));
        }
    }

    stmt->name = stringPool().intern("security.quota_profile.alter." + profile);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAlterExtension() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    StringPool::StringId extension_name = expectIdentifier("Expected extension name");
    std::string extension = std::string(stringPool().get(extension_name));
    std::string payload = captureStatementBody();
    if (payload.empty()) {
        errorCode("PRS_0504", "ALTER EXTENSION requires action clause");
    }

    stmt->name = stringPool().intern("platform.extension.alter." + extension);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAlterPublication() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    StringPool::StringId publication_name = expectIdentifier("Expected publication name");
    std::string publication = std::string(stringPool().get(publication_name));
    std::string payload = captureStatementBody();
    if (payload.empty()) {
        errorCode("PRS_0504", "ALTER PUBLICATION requires action clause");
    }

    stmt->name = stringPool().intern("replication.publication.alter." + publication);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAlterSubscription() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    StringPool::StringId subscription_name = expectIdentifier("Expected subscription name");
    std::string subscription = std::string(stringPool().get(subscription_name));
    std::string payload = captureStatementBody();
    if (payload.empty()) {
        errorCode("PRS_0504", "ALTER SUBSCRIPTION requires action clause");
    }

    stmt->name = stringPool().intern("replication.subscription.alter." + subscription);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAlterReplicationChannel() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    StringPool::StringId channel_name = expectIdentifier("Expected replication channel name");
    std::string channel = std::string(stringPool().get(channel_name));
    std::string payload = captureStatementBody();
    if (payload.empty()) {
        errorCode("PRS_0504", "ALTER REPLICATION CHANNEL requires action clause");
    }

    const std::string payload_upper = toUpperAscii(payload);
    const bool has_one_way = payload_upper.find("ONE_WAY") != std::string::npos ||
                             payload_upper.find("ONEWAY") != std::string::npos;
    const bool has_bidirectional = payload_upper.find("BIDIRECTIONAL") != std::string::npos ||
                                   payload_upper.find("TWO_WAY") != std::string::npos;
    const bool mentions_direction = payload_upper.find("DIRECTION") != std::string::npos;
    if (mentions_direction && !has_one_way && !has_bidirectional) {
        errorCode("PRS_0504", "ALTER REPLICATION CHANNEL DIRECTION requires ONE_WAY or BIDIRECTIONAL");
    }
    if (has_one_way && has_bidirectional) {
        errorCode("PRS_0504", "REPLICATION CHANNEL direction must be either ONE_WAY or BIDIRECTIONAL");
    }

    stmt->name = stringPool().intern("replication.channel.alter." + channel);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAlterCdcTable() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    SchemaPath table_path = parseSchemaPath(state_);
    if (table_path.isEmpty()) {
        errorCode("PRS_0504", "Expected CDC table name");
    }

    std::string table_name = schemaPathToString(table_path, stringPool());
    std::string payload = captureStatementBody();
    if (payload.empty()) {
        errorCode("PRS_0504", "ALTER CDC TABLE requires action clause");
    }

    const std::string payload_upper = toUpperAscii(payload);
    const bool mentions_track = payload_upper.find("TRACK") != std::string::npos;
    const bool has_last_modified = payload_upper.find("LAST_MODIFIED_TXN_ID") != std::string::npos ||
                                   payload_upper.find("LAST_EDIT_TXID") != std::string::npos;
    const bool has_row_uuid = payload_upper.find("ROW_UUID") != std::string::npos;
    if (mentions_track && (!has_last_modified || !has_row_uuid)) {
        errorCode("PRS_0504", "ALTER CDC TABLE TRACK requires LAST_MODIFIED_TXN_ID and ROW_UUID");
    }

    stmt->name = stringPool().intern("etl.cdc.table.alter." + table_name);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAlterDatabaseConnection() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    StringPool::StringId connection_name = expectIdentifier("Expected database connection name");
    std::string connection = std::string(stringPool().get(connection_name));
    std::string payload = captureStatementBody();
    if (payload.empty()) {
        errorCode("PRS_0504", "ALTER DATABASE CONNECTION requires action clause");
    }

    const std::string payload_upper = toUpperAscii(payload);
    if (payload_upper.find("AUTH_MODE") != std::string::npos &&
        payload_upper.find("SHARED") == std::string::npos &&
        payload_upper.find("NAMED") == std::string::npos) {
        errorCode("PRS_0504", "ALTER DATABASE CONNECTION AUTH_MODE requires SHARED or NAMED");
    }

    stmt->name = stringPool().intern("external.database_connection.alter." + connection);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAlterUser() {
    SourceLocation start = currentLocation();
    SchemaPath user_path = parseSchemaPath(state_);
    if (user_path.isEmpty()) {
        error("Expected user name");
        return nullptr;
    }

    if (matchContextual("RENAME")) {
        expectContextual("TO", "Expected TO after RENAME");
        auto* stmt = arena_.create<RenameObjectStmt>();
        stmt->object_type = DdlObjectType::USER;
        stmt->object_path = user_path;
        stmt->new_name = expectIdentifier("Expected new name");
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (match(TokenType::KW_SET)) {
        if (matchContextual("SCHEMA")) {
            auto* stmt = arena_.create<MoveObjectStmt>();
            stmt->object_type = DdlObjectType::USER;
            stmt->object_path = user_path;
            stmt->target_schema = parseSchemaPath(state_);
            stmt->span = makeSpan(start);
            return stmt;
        }
        error("Expected SCHEMA after SET");
        return nullptr;
    }

    auto* stmt = arena_.create<AlterSystemStmt>();
    stmt->name = stringPool().intern("security.user.alter." + schemaPathToString(user_path, stringPool()));

    std::string payload;
    bool saw_option = false;

    while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
        if (match(TokenType::KW_WITH) || matchContextual("WITH")) {
            continue;
        }

        if (matchContextual("PASSWORD")) {
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal for PASSWORD");
                break;
            }
            if (!payload.empty()) {
                payload.push_back(';');
            }
            payload.append("PASSWORD=");
            payload.append(stringPool().get(current().value.string_id));
            advance();
            saw_option = true;
            continue;
        }

        if (matchContextual("SUPERUSER")) {
            if (!payload.empty()) {
                payload.push_back(';');
            }
            payload.append("SUPERUSER=1");
            saw_option = true;
            continue;
        }

        if (matchContextual("NOSUPERUSER")) {
            if (!payload.empty()) {
                payload.push_back(';');
            }
            payload.append("SUPERUSER=0");
            saw_option = true;
            continue;
        }

        break;
    }

    if (!saw_option) {
        error("Expected PASSWORD, SUPERUSER, NOSUPERUSER, RENAME TO, or SET SCHEMA after user name");
    }

    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAlter() {
    ParseModeGuard guard(state_, ParseMode::DDL);

    if (matchContextual("MEASUREMENT") || matchContextual("SCHEDULE")) {
        errorCode("PRS_0505",
                  "Top-level ALTER MEASUREMENT/SCHEDULE is not supported in v3; use ALTER JOB ...");
        return nullptr;
    }
    if (matchContextual("SEARCH")) {
        if (matchContextual("INDEX")) {
            errorCode("PRS_0505",
                      "ALTER SEARCH INDEX is not supported in v3; use ALTER INDEX ...");
        } else {
            errorCode("PRS_0505", "Unsupported SEARCH alter surface");
        }
        return nullptr;
    }
    if (matchContextual("VECTOR")) {
        if (matchContextual("INDEX")) {
            errorCode("PRS_0505",
                      "ALTER VECTOR INDEX is not supported in v3; use ALTER INDEX ...");
        } else {
            errorCode("PRS_0505", "Unsupported VECTOR alter surface");
        }
        return nullptr;
    }
    if (matchContextual("CONNECTION")) {
        if (!matchContextual("RULE")) {
            errorCode("PRS_0505", "Expected RULE after ALTER CONNECTION");
            return nullptr;
        }
        if (!requireFeature(kFeatureSecurityConnectionRuleDdl)) {
            return nullptr;
        }
        return parseAlterConnectionRule();
    }
    if (matchContextual("TOKEN")) {
        if (!requireFeature(kFeatureSecurityTokenDdl)) {
            return nullptr;
        }
        return parseAlterToken();
    }
    if (matchContextual("QUOTA")) {
        if (!matchContextual("PROFILE")) {
            errorCode("PRS_0505", "Expected PROFILE after ALTER QUOTA");
            return nullptr;
        }
        if (!requireFeature(kFeatureSecurityQuotaProfileDdl)) {
            return nullptr;
        }
        return parseAlterQuotaProfile();
    }
    if (matchContextual("EXTENSION")) {
        return parseAlterExtension();
    }
    if (matchContextual("PUBLICATION")) {
        return parseAlterPublication();
    }
    if (matchContextual("SUBSCRIPTION")) {
        return parseAlterSubscription();
    }
    if (matchContextual("REPLICATION")) {
        if (!matchContextual("CHANNEL")) {
            errorCode("PRS_0505", "Expected CHANNEL after ALTER REPLICATION");
            return nullptr;
        }
        return parseAlterReplicationChannel();
    }
    if (matchContextual("CDC")) {
        if (!matchContextual("TABLE")) {
            errorCode("PRS_0505", "Expected TABLE after ALTER CDC");
            return nullptr;
        }
        return parseAlterCdcTable();
    }
    if (matchContextual("CLUSTER")) {
        return parseAlterClusterControl();
    }
    if (matchContextual("CUBE")) {
        return parseAlterCubeControl();
    }

    if (matchContextual("TABLE")) return parseAlterTable();
    if (matchContextual("SCHEMA")) return parseAlterSchema();
    if (matchContextual("DATABASE")) {
        if (matchContextual("CONNECTION")) return parseAlterDatabaseConnection();
        return parseAlterDatabase();
    }
    if (matchContextual("TABLESPACE")) return parseAlterTablespace();
    if (matchContextual("TYPE")) return parseAlterType();
    if (matchContextual("DOMAIN")) return parseAlterDomain();
    if (matchContextual("JOB")) return parseAlterJob();
    if (matchContextual("POLICY")) {
        if (!requireFeature(kFeatureSecurityModelPolicyDdl)) {
            return nullptr;
        }
        return parseAlterPolicy();
    }
    if (matchContextual("SYSTEM")) return parseAlterSystem();

    auto parse_rename_move = [&](DdlObjectType object_type,
                                 bool require_explicit_parent = false,
                                 const char* object_label = "OBJECT") -> Statement* {
        SourceLocation start = currentLocation();

        bool if_exists = false;
        if (match(TokenType::KW_IF)) {
            expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }

        SchemaPath object_path = parseSchemaPath(state_);
        if (require_explicit_parent && object_path.components.size() < 2) {
            errorCode(
                "PRS_0505",
                std::string("ALTER ") + object_label +
                    " requires explicit parent-qualified reference");
            return nullptr;
        }

        if (matchContextual("RENAME")) {
            expectContextual("TO", "Expected TO after RENAME");
            auto* stmt = arena_.create<RenameObjectStmt>();
            stmt->object_type = object_type;
            stmt->if_exists = if_exists;
            stmt->object_path = object_path;
            stmt->new_name = expectIdentifier("Expected new name");
            stmt->span = makeSpan(start);
            return stmt;
        }

        if (match(TokenType::KW_SET)) {
            if (matchContextual("SCHEMA")) {
                auto* stmt = arena_.create<MoveObjectStmt>();
                stmt->object_type = object_type;
                stmt->if_exists = if_exists;
                stmt->object_path = object_path;
                stmt->target_schema = parseSchemaPath(state_);
                stmt->span = makeSpan(start);
                return stmt;
            }
        }

        error("Expected RENAME TO or SET SCHEMA after object name");
        return nullptr;
    };

    auto parse_alter_synonym = [&](bool is_public) -> Statement* {
        SourceLocation start = currentLocation();
        SchemaPath synonym_path = parseSchemaPath(state_);
        if (synonym_path.isEmpty()) {
            error("Expected synonym name");
            return nullptr;
        }

        std::string synonym_name = schemaPathToString(synonym_path, stringPool());
        auto make_payload_literal = [&](const std::string& payload) -> Expression* {
            auto* lit = arena_.create<LiteralExpr>();
            lit->literal_type = LiteralType::STRING;
            lit->string_value = stringPool().intern(payload);
            return lit;
        };

        if (matchContextual("RENAME")) {
            expectContextual("TO", "Expected TO after RENAME");
            auto* stmt = arena_.create<RenameObjectStmt>();
            stmt->object_type = DdlObjectType::SYNONYM;
            stmt->object_path = synonym_path;
            stmt->new_name = expectIdentifier("Expected new synonym name");
            stmt->span = makeSpan(start);
            return stmt;
        }

        if (match(TokenType::KW_SET)) {
            if (matchContextual("SCHEMA")) {
                if (is_public) {
                    errorCode("PRS_0505",
                              "ALTER PUBLIC SYNONYM does not support SET SCHEMA");
                    return nullptr;
                }
                auto* stmt = arena_.create<MoveObjectStmt>();
                stmt->object_type = DdlObjectType::SYNONYM;
                stmt->object_path = synonym_path;
                stmt->target_schema = parseSchemaPath(state_);
                stmt->span = makeSpan(start);
                return stmt;
            }

            if (matchContextual("TARGET")) {
                SchemaPath target_path = parseSchemaPath(state_);
                if (target_path.isEmpty()) {
                    error("Expected target object path after SET TARGET");
                    return nullptr;
                }
                auto* stmt = arena_.create<AlterSystemStmt>();
                stmt->name = stringPool().intern(
                    std::string(is_public ? "synonym.public.set_target." : "synonym.set_target.") +
                    synonym_name);
                stmt->value = make_payload_literal(schemaPathToString(target_path, stringPool()));
                stmt->span = makeSpan(start);
                return stmt;
            }

            if (check(TokenType::LEFT_PAREN)) {
                auto* stmt = arena_.create<AlterSystemStmt>();
                stmt->name = stringPool().intern(
                    std::string(is_public ? "synonym.public.set." : "synonym.set.") + synonym_name);
                stmt->value = make_payload_literal(captureStatementBody());
                stmt->span = makeSpan(start);
                return stmt;
            }

            error("Expected SCHEMA, TARGET, or '(' after SET in ALTER SYNONYM");
            return nullptr;
        }

        if (matchContextual("RESET")) {
            if (!check(TokenType::LEFT_PAREN)) {
                error("Expected '(' after RESET in ALTER SYNONYM");
                return nullptr;
            }
            auto* stmt = arena_.create<AlterSystemStmt>();
            stmt->name = stringPool().intern(
                std::string(is_public ? "synonym.public.reset." : "synonym.reset.") + synonym_name);
            stmt->value = make_payload_literal(captureStatementBody());
            stmt->span = makeSpan(start);
            return stmt;
        }

        error("Expected RENAME, SET, or RESET after synonym name");
        return nullptr;
    };

    if (matchContextual("VIEW")) return parse_rename_move(DdlObjectType::VIEW);
    if (matchContextual("INDEX")) {
        SourceLocation start = currentLocation();

        auto parse_option_value = [&]() -> StringPool::StringId {
            if (match(TokenType::KW_TRUE)) return stringPool().intern("true");
            if (match(TokenType::KW_FALSE)) return stringPool().intern("false");
            if (check(TokenType::INTEGER_LITERAL)) {
                auto id = stringPool().intern(std::to_string(current().value.int_value));
                advance();
                return id;
            }
            if (check(TokenType::FLOAT_LITERAL)) {
                auto id = stringPool().intern(std::to_string(current().value.float_value));
                advance();
                return id;
            }
            if (check(TokenType::STRING_LITERAL) || isIdentifier()) {
                auto id = current().value.string_id;
                advance();
                return id;
            }
            error("Expected scalar value for index option");
            return StringPool::INVALID_ID;
        };

        auto parse_option_set_list = [&](AlterIndexStmt* stmt, const std::string& action_label) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after " + action_label);
            while (!check(TokenType::RIGHT_PAREN) &&
                   !check(TokenType::SEMICOLON) &&
                   !check(TokenType::END_OF_FILE)) {
                if (!isIdentifier()) {
                    error("Expected index option name");
                    break;
                }

                auto opt_name_id = current().value.string_id;
                auto opt_name = stringPool().get(opt_name_id);
                advance();
                expect(TokenType::EQUAL, "Expected '=' after index option name");
                auto opt_value_id = parse_option_value();
                if (opt_value_id == StringPool::INVALID_ID) {
                    break;
                }
                stmt->option_assignments.push_back({opt_name_id, opt_value_id});

                auto opt_value = stringPool().get(opt_value_id);
                if (caseInsensitiveEquals(opt_name, "BLOOM_FILTER")) {
                    stmt->options.bloom_filter_enabled =
                        caseInsensitiveEquals(opt_value, "true") || opt_value == "1";
                    stmt->options.bloom_filter_set = true;
                } else if (caseInsensitiveEquals(opt_name, "BLOOM_FPR")) {
                    try {
                        stmt->options.bloom_fpr = std::stod(std::string(opt_value));
                        stmt->options.bloom_fpr_set = true;
                    } catch (...) {
                        error("Expected numeric value for BLOOM_FPR");
                    }
                }

                if (!match(TokenType::COMMA)) {
                    break;
                }
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after index options");
        };

        auto parse_option_reset_list = [&](AlterIndexStmt* stmt, const std::string& action_label) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after " + action_label);
            do {
                stmt->reset_options.push_back(expectIdentifier("Expected index option name"));
            } while (match(TokenType::COMMA));
            expect(TokenType::RIGHT_PAREN, "Expected ')' after index option list");
        };

        auto parse_maintenance_mode = [&](AlterIndexStmt* stmt) {
            if (matchContextual("ONLINE")) {
                stmt->mode = IndexMaintenanceMode::ONLINE;
            } else if (matchContextual("OFFLINE")) {
                stmt->mode = IndexMaintenanceMode::OFFLINE;
            }
        };

        if (matchContextual("DEFAULTS")) {
            auto* stmt = arena_.create<AlterIndexStmt>();
            stmt->defaults_scope = true;

            expectContextual("FOR", "Expected FOR after ALTER INDEX DEFAULTS");
            if (!isIdentifier()) {
                error("Expected index type after ALTER INDEX DEFAULTS FOR");
            } else {
                stmt->defaults_index_type_name = current().value.string_id;
                auto parsed = indexTypeFromName(stringPool().get(stmt->defaults_index_type_name));
                if (!parsed.has_value()) {
                    error("Unknown index type");
                } else {
                    stmt->defaults_index_type = *parsed;
                }
                advance();
            }

            if (match(TokenType::KW_SET)) {
                stmt->action = AlterIndexAction::SET_OPTIONS;
                parse_option_set_list(stmt, "SET");
            } else if (matchContextual("RESET")) {
                stmt->action = AlterIndexAction::RESET_OPTIONS;
                parse_option_reset_list(stmt, "RESET");
            } else {
                error("Expected SET or RESET after ALTER INDEX DEFAULTS FOR <index_type>");
            }

            stmt->span = makeSpan(start);
            return stmt;
        }

        bool if_exists = false;
        if (match(TokenType::KW_IF)) {
            expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
            if_exists = true;
        }

        SchemaPath index_path = parseSchemaPath(state_);
        if (index_path.isEmpty()) {
            error("Expected index name");
            return nullptr;
        }
        if (index_path.components.size() < 2) {
            errorCode("PRS_0505",
                      "ALTER INDEX requires explicit parent-qualified index reference");
            return nullptr;
        }

        if (matchContextual("RENAME")) {
            expectContextual("TO", "Expected TO after RENAME");
            auto* stmt = arena_.create<RenameObjectStmt>();
            stmt->object_type = DdlObjectType::INDEX;
            stmt->if_exists = if_exists;
            stmt->object_path = index_path;
            stmt->new_name = expectIdentifier("Expected new name");
            stmt->span = makeSpan(start);
            return stmt;
        }

        if (match(TokenType::KW_SET)) {
            if (matchContextual("SCHEMA")) {
                auto* stmt = arena_.create<MoveObjectStmt>();
                stmt->object_type = DdlObjectType::INDEX;
                stmt->if_exists = if_exists;
                stmt->object_path = index_path;
                stmt->target_schema = parseSchemaPath(state_);
                stmt->span = makeSpan(start);
                return stmt;
            }

            auto* stmt = arena_.create<AlterIndexStmt>();
            stmt->index_path = index_path;
            stmt->action = AlterIndexAction::SET_OPTIONS;
            parse_option_set_list(stmt, "SET");
            stmt->span = makeSpan(start);
            return stmt;
        }

        if (matchContextual("RESET")) {
            auto* stmt = arena_.create<AlterIndexStmt>();
            stmt->index_path = index_path;
            stmt->action = AlterIndexAction::RESET_OPTIONS;
            parse_option_reset_list(stmt, "RESET");
            stmt->span = makeSpan(start);
            return stmt;
        }

        if (matchContextual("REBUILD")) {
            auto* stmt = arena_.create<AlterIndexStmt>();
            stmt->index_path = index_path;
            stmt->action = AlterIndexAction::REBUILD;
            parse_maintenance_mode(stmt);
            if (match(TokenType::KW_WITH)) {
                parse_option_set_list(stmt, "WITH");
            }
            stmt->span = makeSpan(start);
            return stmt;
        }

        if (matchContextual("REBALANCE")) {
            auto* stmt = arena_.create<AlterIndexStmt>();
            stmt->index_path = index_path;
            stmt->action = AlterIndexAction::REBALANCE;
            parse_maintenance_mode(stmt);
            if (match(TokenType::KW_WITH)) {
                parse_option_set_list(stmt, "WITH");
            }
            stmt->span = makeSpan(start);
            return stmt;
        }

        if (matchContextual("RELOCATE")) {
            auto* stmt = arena_.create<AlterIndexStmt>();
            stmt->index_path = index_path;
            stmt->action = AlterIndexAction::RELOCATE;
            expectContextual("TO", "Expected TO after RELOCATE");
            if (!matchContextual("FILESPACE") && !matchContextual("TABLESPACE")) {
                error("Expected FILESPACE after RELOCATE TO");
            }
            stmt->target_filespace = parseSchemaPath(state_);
            stmt->has_target_filespace = !stmt->target_filespace.isEmpty();
            if (stmt->has_target_filespace && stmt->target_filespace.components.size() != 1) {
                errorCode("PRS_0505",
                          "FILESPACE/TABLESPACE names are database-scoped and must not be schema-qualified");
            }
            parse_maintenance_mode(stmt);
            if (match(TokenType::KW_WITH)) {
                parse_option_set_list(stmt, "WITH");
            }
            stmt->span = makeSpan(start);
            return stmt;
        }

        if (matchContextual("LIGHT")) {
            auto* stmt = arena_.create<AlterIndexStmt>();
            stmt->index_path = index_path;
            stmt->action = AlterIndexAction::LIGHT_SCAN;
            expectContextual("SCAN", "Expected SCAN after LIGHT");
            if (match(TokenType::KW_WITH)) {
                parse_option_set_list(stmt, "WITH");
            }
            stmt->span = makeSpan(start);
            return stmt;
        }

        if (matchContextual("DIAGNOSTIC")) {
            auto* stmt = arena_.create<AlterIndexStmt>();
            stmt->index_path = index_path;
            stmt->action = AlterIndexAction::DIAGNOSTIC_SCAN;
            expectContextual("SCAN", "Expected SCAN after DIAGNOSTIC");
            if (match(TokenType::KW_WITH)) {
                parse_option_set_list(stmt, "WITH");
            }
            stmt->span = makeSpan(start);
            return stmt;
        }

        error("Expected RENAME TO, SET, RESET, REBUILD, REBALANCE, RELOCATE, LIGHT SCAN, or DIAGNOSTIC SCAN");
        return nullptr;
    }
    if (matchContextual("SEQUENCE")) return parse_rename_move(DdlObjectType::SEQUENCE);
    if (matchContextual("DOMAIN")) return parse_rename_move(DdlObjectType::DOMAIN);
    if (matchContextual("TRIGGER")) {
        return parse_rename_move(DdlObjectType::TRIGGER, true, "TRIGGER");
    }
    if (matchContextual("FUNCTION")) return parse_rename_move(DdlObjectType::FUNCTION);
    if (matchContextual("PROCEDURE")) return parse_rename_move(DdlObjectType::PROCEDURE);
    if (matchContextual("PACKAGE")) return parse_rename_move(DdlObjectType::PACKAGE);
    if (matchContextual("EXCEPTION")) return parse_rename_move(DdlObjectType::EXCEPTION);
    if (matchContextual("UDR")) return parse_rename_move(DdlObjectType::UDR);
    if (matchContextual("TABLESPACE")) return parse_rename_move(DdlObjectType::TABLESPACE);
    if (matchContextual("ROLE")) return parse_rename_move(DdlObjectType::ROLE);
    if (matchContextual("USER")) {
        if (!requireFeature(kFeatureSecurityUserAccountDdl)) {
            return nullptr;
        }
        return parseAlterUser();
    }
    if (matchContextual("GROUP")) return parse_rename_move(DdlObjectType::GROUP);
    if (matchContextual("PUBLIC")) {
        if (matchContextual("SYNONYM")) {
            return parse_alter_synonym(true);
        }
        errorCode("PRS_0505", "Expected SYNONYM after ALTER PUBLIC");
        return nullptr;
    }
    if (matchContextual("SYNONYM")) return parse_alter_synonym(false);
    if (matchContextual("SERVER")) return parse_rename_move(DdlObjectType::FOREIGN_SERVER);
    if (matchContextual("FOREIGN")) {
        expectContextual("TABLE", "Expected TABLE after FOREIGN");
        return parse_rename_move(DdlObjectType::FOREIGN_TABLE);
    }

    error("Expected object type after ALTER");
    return nullptr;
}

AlterSchemaStmt* Parser::parseAlterSchema() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<AlterSchemaStmt>();
    stmt->schema_path = parseSchemaPath(state_);
    if (stmt->schema_path.isEmpty()) {
        error("Expected schema name");
    }

    if (matchContextual("RENAME")) {
        expectContextual("TO", "Expected TO after RENAME");
        stmt->action = AlterSchemaAction::RENAME;
        stmt->new_name = expectIdentifier("Expected new schema name");
    } else if (matchContextual("OWNER")) {
        expectContextual("TO", "Expected TO after OWNER");
        stmt->action = AlterSchemaAction::SET_OWNER;
        stmt->owner = expectIdentifier("Expected owner name");
    } else if (matchContextual("SET")) {
        if (matchContextual("PATH")) {
            stmt->action = AlterSchemaAction::SET_PATH;
            stmt->new_path = parseSchemaPath(state_);
        } else {
            error("Expected PATH after SET");
        }
    } else {
        error("Expected RENAME TO or OWNER TO after schema name");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

AlterDatabaseStmt* Parser::parseAlterDatabase() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<AlterDatabaseStmt>();
    stmt->database_path = parseSchemaPath(state_);
    if (stmt->database_path.isEmpty()) {
        error("Expected database name");
    }

    if (matchContextual("RENAME")) {
        expectContextual("TO", "Expected TO after RENAME");
        stmt->action = AlterDatabaseAction::RENAME;
        stmt->new_name = expectIdentifier("Expected new database name");
    } else if (matchContextual("OWNER")) {
        expectContextual("TO", "Expected TO after OWNER");
        stmt->action = AlterDatabaseAction::SET_OWNER;
        stmt->owner = expectIdentifier("Expected owner name");
    } else if (matchContextual("ALIAS")) {
        if (matchContextual("ADD")) {
            stmt->action = AlterDatabaseAction::ADD_ALIAS;
            stmt->alias = expectIdentifier("Expected alias name");
        } else if (matchContextual("DROP")) {
            stmt->action = AlterDatabaseAction::DROP_ALIAS;
            stmt->alias = expectIdentifier("Expected alias name");
        } else {
            error("Expected ADD or DROP after ALIAS");
        }
    } else {
        error("Expected RENAME TO, OWNER TO, or ALIAS ADD/DROP after database name");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

AlterTypeStmt* Parser::parseAlterType() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<AlterTypeStmt>();
    stmt->type_path = parseSchemaPath(state_);
    if (stmt->type_path.isEmpty()) {
        error("Expected type name");
    }

    if (matchContextual("RENAME")) {
        if (matchContextual("VALUE")) {
            stmt->action = AlterTypeAction::RENAME_VALUE;
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal after RENAME VALUE");
            } else {
                stmt->old_label = current().value.string_id;
                advance();
            }
            expectContextual("TO", "Expected TO after RENAME VALUE");
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal after TO");
            } else {
                stmt->new_label = current().value.string_id;
                advance();
            }
        } else {
            expectContextual("TO", "Expected TO after RENAME");
            stmt->action = AlterTypeAction::RENAME_TO;
            stmt->new_name = expectIdentifier("Expected new type name");
        }
    } else if (match(TokenType::KW_SET)) {
        if (matchContextual("SCHEMA")) {
            stmt->action = AlterTypeAction::SET_SCHEMA;
            stmt->new_schema = expectIdentifier("Expected schema name");
        } else if (check(TokenType::LEFT_PAREN)) {
            stmt->action = AlterTypeAction::SET_OPTIONS;
            expect(TokenType::LEFT_PAREN, "Expected '(' after SET");
            while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
                StringPool::StringId key_id = expectIdentifier("Expected option name");
                std::string key = std::string(stringPool().get(key_id));
                expect(TokenType::EQUAL, "Expected '=' after option name");
                if (key == "SUBTYPE") {
                    stmt->is_range_options = true;
                    stmt->range_options.subtype = parseTypeName();
                    stmt->range_options.has_subtype = true;
                } else if (key == "SUBTYPE_COLLATION") {
                    stmt->is_range_options = true;
                    stmt->range_options.subtype_collation = std::string(stringPool().get(expectIdentifier("Expected collation name")));
                    stmt->range_options.has_subtype_collation = true;
                } else if (key == "SUBTYPE_OPCLASS") {
                    stmt->is_range_options = true;
                    stmt->range_options.subtype_opclass = std::string(stringPool().get(expectIdentifier("Expected opclass name")));
                    stmt->range_options.has_subtype_opclass = true;
                } else if (key == "CANONICAL") {
                    stmt->is_range_options = true;
                    stmt->range_options.canonical = std::string(stringPool().get(expectIdentifier("Expected function name")));
                    stmt->range_options.has_canonical = true;
                } else if (key == "SUBTYPE_DIFF") {
                    stmt->is_range_options = true;
                    stmt->range_options.subtype_diff = std::string(stringPool().get(expectIdentifier("Expected function name")));
                    stmt->range_options.has_subtype_diff = true;
                } else if (key == "MULTIRANGE") {
                    stmt->is_range_options = true;
                    if (matchContextual("TRUE")) {
                        stmt->range_options.multirange = true;
                    } else if (matchContextual("FALSE")) {
                        stmt->range_options.multirange = false;
                    } else if (check(TokenType::INTEGER_LITERAL)) {
                        stmt->range_options.multirange = current().value.int_value != 0;
                        advance();
                    } else {
                        error("Expected TRUE/FALSE for MULTIRANGE");
                    }
                    stmt->range_options.has_multirange = true;
                } else if (key == "STORAGE") {
                    stmt->is_base_options = true;
                    stmt->base_options.storage = parseTypeName();
                    stmt->base_options.has_storage = true;
                } else if (key == "INPUT") {
                    stmt->is_base_options = true;
                    stmt->base_options.input_function = std::string(stringPool().get(expectIdentifier("Expected function name")));
                } else if (key == "OUTPUT") {
                    stmt->is_base_options = true;
                    stmt->base_options.output_function = std::string(stringPool().get(expectIdentifier("Expected function name")));
                } else if (key == "RECEIVE") {
                    stmt->is_base_options = true;
                    stmt->base_options.receive_function = std::string(stringPool().get(expectIdentifier("Expected function name")));
                    stmt->base_options.has_receive = true;
                } else if (key == "SEND") {
                    stmt->is_base_options = true;
                    stmt->base_options.send_function = std::string(stringPool().get(expectIdentifier("Expected function name")));
                    stmt->base_options.has_send = true;
                } else if (key == "TYPMOD_IN") {
                    stmt->is_base_options = true;
                    stmt->base_options.typmod_in_function = std::string(stringPool().get(expectIdentifier("Expected function name")));
                    stmt->base_options.has_typmod_in = true;
                } else if (key == "TYPMOD_OUT") {
                    stmt->is_base_options = true;
                    stmt->base_options.typmod_out_function = std::string(stringPool().get(expectIdentifier("Expected function name")));
                    stmt->base_options.has_typmod_out = true;
                } else if (key == "ANALYZE") {
                    stmt->is_base_options = true;
                    stmt->base_options.analyze_function = std::string(stringPool().get(expectIdentifier("Expected function name")));
                    stmt->base_options.has_analyze = true;
                } else if (key == "ALIGNMENT") {
                    stmt->is_base_options = true;
                    if (matchContextual("CHAR")) {
                        stmt->base_options.alignment = BaseTypeAlignment::CHAR;
                    } else if (matchContextual("SHORT")) {
                        stmt->base_options.alignment = BaseTypeAlignment::SHORT;
                    } else if (matchContextual("INT")) {
                        stmt->base_options.alignment = BaseTypeAlignment::INT;
                    } else if (matchContextual("DOUBLE")) {
                        stmt->base_options.alignment = BaseTypeAlignment::DOUBLE;
                    } else {
                        error("Expected CHAR/SHORT/INT/DOUBLE for ALIGNMENT");
                    }
                    stmt->base_options.has_alignment = true;
                } else if (key == "STORAGE_MODE") {
                    stmt->is_base_options = true;
                    if (matchContextual("PLAIN")) {
                        stmt->base_options.storage_mode = BaseTypeStorageMode::PLAIN;
                    } else if (matchContextual("EXTERNAL")) {
                        stmt->base_options.storage_mode = BaseTypeStorageMode::EXTERNAL;
                    } else if (matchContextual("EXTENDED")) {
                        stmt->base_options.storage_mode = BaseTypeStorageMode::EXTENDED;
                    } else if (matchContextual("MAIN")) {
                        stmt->base_options.storage_mode = BaseTypeStorageMode::MAIN;
                    } else {
                        error("Expected PLAIN/EXTERNAL/EXTENDED/MAIN for STORAGE_MODE");
                    }
                    stmt->base_options.has_storage_mode = true;
                } else if (key == "CATEGORY") {
                    stmt->is_base_options = true;
                    if (check(TokenType::STRING_LITERAL)) {
                        auto text = std::string(stringPool().get(current().value.string_id));
                        advance();
                        if (!text.empty()) {
                            stmt->base_options.category = text[0];
                            stmt->base_options.has_category = true;
                        }
                    } else {
                        error("Expected string literal for CATEGORY");
                    }
                } else if (key == "PREFERRED") {
                    stmt->is_base_options = true;
                    if (matchContextual("TRUE")) {
                        stmt->base_options.preferred = true;
                    } else if (matchContextual("FALSE")) {
                        stmt->base_options.preferred = false;
                    } else if (check(TokenType::INTEGER_LITERAL)) {
                        stmt->base_options.preferred = current().value.int_value != 0;
                        advance();
                    } else {
                        error("Expected TRUE/FALSE for PREFERRED");
                    }
                    stmt->base_options.has_preferred = true;
                } else {
                    error("Unsupported ALTER TYPE option: " + key);
                }

                if (!match(TokenType::COMMA)) {
                    break;
                }
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after SET options");
        } else {
            error("Expected SCHEMA or '(' after SET");
        }
    } else if (matchContextual("ADD")) {
        expectContextual("VALUE", "Expected VALUE after ADD");
        stmt->action = AlterTypeAction::ADD_VALUE;
        if (!check(TokenType::STRING_LITERAL)) {
            error("Expected string literal after ADD VALUE");
        } else {
            stmt->value_label = current().value.string_id;
            advance();
        }
        if (matchContextual("BEFORE")) {
            stmt->has_before = true;
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal after BEFORE");
            } else {
                stmt->before_label = current().value.string_id;
                advance();
            }
        } else if (matchContextual("AFTER")) {
            stmt->has_after = true;
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal after AFTER");
            } else {
                stmt->after_label = current().value.string_id;
                advance();
            }
        }
    } else if (matchContextual("FINALIZE")) {
        stmt->action = AlterTypeAction::FINALIZE;
    } else {
        error("Expected RENAME, SET, ADD VALUE, or FINALIZE after type name");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

AlterDomainStmt* Parser::parseAlterDomain() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<AlterDomainStmt>();
    stmt->domain_path = parseSchemaPath(state_);
    if (stmt->domain_path.isEmpty()) {
        error("Expected domain name");
    } else if (stmt->domain_path.components.size() != 1) {
        errorCode("PRS_0505",
                  "DOMAIN names are global and must not be schema-qualified");
    }

    if (match(TokenType::KW_SET)) {
        if (match(TokenType::KW_DEFAULT)) {
            stmt->action = AlterDomainAction::SET_DEFAULT;
            stmt->value = extractExpressionText(parseExpression());
        } else if (matchContextual("COMPAT")) {
            stmt->action = AlterDomainAction::SET_COMPAT;
            if (check(TokenType::STRING_LITERAL)) {
                stmt->value = std::string(stringPool().get(current().value.string_id));
                advance();
            } else {
                stmt->value = std::string(stringPool().get(expectIdentifier("Expected compat name")));
            }
        } else {
            error("Expected DEFAULT or COMPAT after SET");
        }
    } else if (match(TokenType::KW_DROP)) {
        if (match(TokenType::KW_DEFAULT)) {
            stmt->action = AlterDomainAction::DROP_DEFAULT;
        } else if (matchContextual("CONSTRAINT")) {
            stmt->action = AlterDomainAction::DROP_CONSTRAINT;
            stmt->constraint_name = expectIdentifier("Expected constraint name");
        } else if (matchContextual("COMPAT")) {
            stmt->action = AlterDomainAction::DROP_COMPAT;
        } else {
            error("Expected DEFAULT, CONSTRAINT, or COMPAT after DROP");
        }
    } else if (matchContextual("ADD")) {
        if (matchContextual("CHECK")) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after CHECK");
            stmt->action = AlterDomainAction::ADD_CHECK;
            stmt->value = extractExpressionText(parseExpression());
            expect(TokenType::RIGHT_PAREN, "Expected ')' after CHECK expression");
        } else {
            error("Expected CHECK after ADD");
        }
    } else if (matchContextual("RENAME")) {
        expectContextual("TO", "Expected TO after RENAME");
        stmt->action = AlterDomainAction::RENAME;
        stmt->new_name = expectIdentifier("Expected new domain name");
    } else {
        error("Expected SET, DROP, ADD, or RENAME after domain name");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

AlterPolicyStmt* Parser::parseAlterPolicy() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<AlterPolicyStmt>();
    stmt->policy_name = expectIdentifier("Expected policy name");

    expect(TokenType::KW_ON, "Expected ON after policy name");
    stmt->table_path = parseSchemaPath(state_);

    // TO { role_name | PUBLIC } [, ...]
    if (matchContextual("TO")) {
        do {
            if (matchContextual("PUBLIC")) {
                // PUBLIC means all roles
                stmt->roles.clear();
            } else {
                StringPool::StringId role_name = expectIdentifier("Expected role name");
                stmt->roles.push_back(role_name);
            }
        } while (match(TokenType::COMMA));
    }

    // USING ( expression )
    if (matchContextual("USING")) {
        if (match(TokenType::LEFT_PAREN)) {
            stmt->using_expr = parseExpression();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after USING expression");
        } else {
            stmt->using_expr = parseExpression();
        }
    }

    // WITH CHECK ( expression )
    if (matchContextual("WITH")) {
        expectContextual("CHECK", "Expected CHECK after WITH");
        if (match(TokenType::LEFT_PAREN)) {
            stmt->with_check_expr = parseExpression();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after WITH CHECK expression");
        } else {
            stmt->with_check_expr = parseExpression();
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

AlterJobStmt* Parser::parseAlterJob() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<AlterJobStmt>();
    stmt->job_name = expectIdentifier("Expected job name");

    auto parse_duration_seconds = [&](const char* context) -> uint32_t {
        if (!check(TokenType::INTEGER_LITERAL)) {
            error(std::string("Expected duration seconds for ") + context);
            return 0;
        }
        int64_t value = current().value.int_value;
        advance();
        if (value < 0) {
            error(std::string("Duration must be non-negative for ") + context);
            return 0;
        }

        uint64_t multiplier = 1;
        if (isIdentifier()) {
            std::string unit = std::string(stringPool().get(current().value.string_id));
            advance();
            if (caseInsensitiveEquals(unit, "S") || caseInsensitiveEquals(unit, "SEC") ||
                caseInsensitiveEquals(unit, "SECOND") || caseInsensitiveEquals(unit, "SECONDS")) {
                multiplier = 1;
            } else if (caseInsensitiveEquals(unit, "M") || caseInsensitiveEquals(unit, "MIN") ||
                       caseInsensitiveEquals(unit, "MINUTE") || caseInsensitiveEquals(unit, "MINUTES")) {
                multiplier = 60;
            } else if (caseInsensitiveEquals(unit, "H") || caseInsensitiveEquals(unit, "HOUR") ||
                       caseInsensitiveEquals(unit, "HOURS")) {
                multiplier = 3600;
            } else if (caseInsensitiveEquals(unit, "D") || caseInsensitiveEquals(unit, "DAY") ||
                       caseInsensitiveEquals(unit, "DAYS")) {
                multiplier = 86400;
            } else {
                error(std::string("Unknown duration unit for ") + context);
                return static_cast<uint32_t>(value);
            }
        }

        uint64_t seconds = static_cast<uint64_t>(value) * multiplier;
        if (seconds > std::numeric_limits<uint32_t>::max()) {
            error(std::string("Duration too large for ") + context);
            return std::numeric_limits<uint32_t>::max();
        }
        return static_cast<uint32_t>(seconds);
    };

    auto parse_timestamp_literal = [&](const char* context) -> StringPool::StringId {
        if (!check(TokenType::STRING_LITERAL)) {
            error(std::string("Expected timestamp string for ") + context);
            return StringPool::INVALID_ID;
        }
        auto id = current().value.string_id;
        advance();
        return id;
    };

    auto canonicalize_rrule = [&](StringPool::StringId raw_id,
                                  StringPool::StringId& canonical_id) -> bool {
        if (raw_id == StringPool::INVALID_ID) {
            return false;
        }

        auto trim = [](std::string_view s) -> std::string_view {
            size_t b = 0;
            while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b]))) {
                ++b;
            }
            size_t e = s.size();
            while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
                --e;
            }
            return s.substr(b, e - b);
        };

        std::set<std::string> seen_keys;
        std::vector<std::pair<std::string, std::string>> kv_pairs;
        std::string raw = std::string(stringPool().get(raw_id));
        size_t pos = 0;
        while (pos < raw.size()) {
            size_t next = raw.find(';', pos);
            std::string token = std::string(trim(std::string_view(raw).substr(
                pos, next == std::string::npos ? std::string::npos : next - pos)));
            if (token.empty()) {
                errorCode("PRS_0507", "Invalid RRULE token");
                return false;
            }
            size_t eq = token.find('=');
            if (eq == std::string::npos || eq == 0 || eq + 1 >= token.size()) {
                errorCode("PRS_0507", "Invalid RRULE key/value contract");
                return false;
            }

            std::string key = std::string(trim(token.substr(0, eq)));
            std::string value = std::string(trim(token.substr(eq + 1)));
            for (char& c : key) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }

            static const char* kAllowed[] = {
                "FREQ", "INTERVAL", "COUNT", "UNTIL", "BYSECOND", "BYMINUTE", "BYHOUR",
                "BYDAY", "BYMONTHDAY", "BYYEARDAY", "BYWEEKNO", "BYMONTH", "BYSETPOS", "WKST"
            };
            bool allowed = false;
            for (const char* candidate : kAllowed) {
                if (key == candidate) {
                    allowed = true;
                    break;
                }
            }
            if (!allowed) {
                errorCode("PRS_0507", "Invalid RRULE key");
                return false;
            }
            if (!seen_keys.insert(key).second) {
                errorCode("PRS_0507", "Duplicate RRULE key");
                return false;
            }

            kv_pairs.push_back({std::move(key), std::move(value)});
            if (next == std::string::npos) {
                break;
            }
            pos = next + 1;
        }

        if (seen_keys.find("FREQ") == seen_keys.end()) {
            errorCode("PRS_0507", "RRULE requires FREQ");
            return false;
        }
        if (seen_keys.size() < 2) {
            errorCode("PRS_0507",
                      "RRULE requires at least one scheduling constraint beyond FREQ");
            return false;
        }

        std::sort(kv_pairs.begin(), kv_pairs.end(),
                  [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });

        std::string canonical;
        for (size_t i = 0; i < kv_pairs.size(); ++i) {
            if (i != 0) {
                canonical.push_back(';');
            }
            canonical.append(kv_pairs[i].first);
            canonical.push_back('=');
            canonical.append(kv_pairs[i].second);
        }
        canonical_id = stringPool().intern(canonical);
        return true;
    };

    auto parse_schedule = [&]() {
        if (matchContextual("SCHEDULE")) {
            expect(TokenType::EQUAL, "Expected '=' after SCHEDULE");
        }

        if (matchContextual("CRON")) {
            if (!requireFeature(kFeatureScheduleRruleSurface)) {
                return false;
            }
            stmt->schedule_kind = JobScheduleKind::CRON;
            auto raw = parse_timestamp_literal("CRON");
            auto canonical = raw;
            canonicalize_rrule(raw, canonical);
            stmt->cron_expression = canonical;
            stmt->has_schedule = true;
            return true;
        }
        if (matchContextual("AT")) {
            stmt->schedule_kind = JobScheduleKind::AT;
            stmt->at_timestamp = parse_timestamp_literal("AT");
            stmt->has_schedule = true;
            return true;
        }
        if (matchContextual("EVERY")) {
            stmt->schedule_kind = JobScheduleKind::EVERY;
            stmt->interval_seconds = parse_duration_seconds("EVERY");
            if (matchContextual("STARTS")) {
                stmt->starts_at = parse_timestamp_literal("STARTS");
            }
            if (matchContextual("ENDS")) {
                stmt->ends_at = parse_timestamp_literal("ENDS");
            }
            stmt->has_schedule = true;
            return true;
        }

        return false;
    };

    auto parse_partition_list = [&](const char* context) -> StringPool::StringId {
        expect(TokenType::LEFT_PAREN, std::string("Expected '(' after ").append(context));
        std::string combined;
        bool first = true;
        while (!check(TokenType::RIGHT_PAREN) && !check(TokenType::END_OF_FILE)) {
            if (!check(TokenType::STRING_LITERAL) && !check(TokenType::IDENTIFIER)) {
                error(std::string("Expected shard identifier or string for ").append(context));
                break;
            }
            auto text = stringPool().get(current().value.string_id);
            advance();
            if (!first) {
                combined.push_back(',');
            }
            combined.append(text);
            first = false;
            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, std::string("Expected ')' after ").append(context));
        if (combined.empty()) {
            error(std::string(context).append(" requires at least one shard"));
            return StringPool::INVALID_ID;
        }
        return stringPool().intern(combined);
    };

    auto parse_partition_expression = [&](const char* context) -> StringPool::StringId {
        if (check(TokenType::STRING_LITERAL) || check(TokenType::IDENTIFIER)) {
            auto id = current().value.string_id;
            advance();
            return id;
        }
        if (match(TokenType::LEFT_PAREN)) {
            if (check(TokenType::RIGHT_PAREN)) {
                error(std::string(context).append(" expression cannot be empty"));
                advance();
                return StringPool::INVALID_ID;
            }

            std::string_view input = state_.lexer().input();
            size_t start = current().span.start.offset;
            size_t end = start;
            bool saw_token = false;
            int depth = 1;
            Token last = current();

            while (!isAtEnd()) {
                if (check(TokenType::LEFT_PAREN)) {
                    depth++;
                } else if (check(TokenType::RIGHT_PAREN)) {
                    depth--;
                    if (depth == 0) {
                        break;
                    }
                }
                last = current();
                saw_token = true;
                advance();
            }

            if (saw_token) {
                end = last.span.start.offset + last.span.length;
                if (end > input.size()) {
                    end = input.size();
                }
            }

            expect(TokenType::RIGHT_PAREN, std::string("Expected ')' after ").append(context));
            if (!saw_token || end <= start) {
                error(std::string(context).append(" expression cannot be empty"));
                return StringPool::INVALID_ID;
            }

            std::string_view text = input.substr(start, end - start);
            size_t trim_start = text.find_first_not_of(" \t\r\n");
            if (trim_start == std::string_view::npos) {
                error(std::string(context).append(" expression cannot be empty"));
                return StringPool::INVALID_ID;
            }
            size_t trim_end = text.find_last_not_of(" \t\r\n");
            return stringPool().intern(text.substr(trim_start, trim_end - trim_start + 1));
        }

        error(std::string("Expected expression for ").append(context));
        return StringPool::INVALID_ID;
    };

    auto parse_partition = [&]() {
        if (!matchContextual("PARTITION")) {
            return false;
        }
        expectContextual("BY", "Expected BY after PARTITION");
        stmt->has_partition = true;
        if (matchContextual("ALL_SHARDS")) {
            stmt->partition_strategy = stringPool().intern("ALL_SHARDS");
        } else if (matchContextual("SINGLE_SHARD")) {
            stmt->partition_strategy = stringPool().intern("SINGLE_SHARD");
            stmt->partition_shard = parse_timestamp_literal("SINGLE_SHARD");
        } else if (matchContextual("SHARD_SET")) {
            stmt->partition_strategy = stringPool().intern("SHARD_SET");
            if (check(TokenType::LEFT_PAREN)) {
                stmt->partition_expression = parse_partition_list("SHARD_SET");
            } else {
                stmt->partition_expression = parse_partition_expression("SHARD_SET");
            }
        } else if (matchContextual("DYNAMIC")) {
            stmt->partition_strategy = stringPool().intern("DYNAMIC");
            stmt->partition_expression = parse_partition_expression("DYNAMIC");
        } else {
            error("Expected partition strategy after PARTITION BY");
        }
        return true;
    };

    auto parse_measurement_key = [&]() -> StringPool::StringId {
        if (matchContextual("ENABLED")) {
            return stringPool().intern("ENABLED");
        }
        if (matchContextual("WINDOW")) {
            return stringPool().intern("WINDOW");
        }
        if (matchContextual("RETENTION")) {
            return stringPool().intern("RETENTION");
        }
        if (matchContextual("GRANULARITY")) {
            return stringPool().intern("GRANULARITY");
        }
        if (!isIdentifier()) {
            error("Expected MEASUREMENT option key");
            return StringPool::INVALID_ID;
        }
        auto key = current().value.string_id;
        advance();
        return key;
    };

    auto parse_measurement_value = [&]() -> StringPool::StringId {
        if (check(TokenType::COMMA) || check(TokenType::RIGHT_PAREN)) {
            error("Expected MEASUREMENT option value");
            return StringPool::INVALID_ID;
        }

        std::string_view input = state_.lexer().input();
        size_t start = current().span.start.offset;
        size_t end = start;
        bool saw_token = false;
        int depth = 0;
        Token last = current();

        while (!isAtEnd()) {
            if (check(TokenType::COMMA) && depth == 0) {
                break;
            }
            if (check(TokenType::RIGHT_PAREN) && depth == 0) {
                break;
            }
            if (check(TokenType::LEFT_PAREN)) {
                depth++;
            } else if (check(TokenType::RIGHT_PAREN) && depth > 0) {
                depth--;
            }
            last = current();
            saw_token = true;
            advance();
        }

        if (saw_token) {
            end = last.span.start.offset + last.span.length;
            if (end > input.size()) {
                end = input.size();
            }
        }

        if (!saw_token || end <= start) {
            error("Expected MEASUREMENT option value");
            return StringPool::INVALID_ID;
        }

        std::string_view text = input.substr(start, end - start);
        size_t trim_start = text.find_first_not_of(" \t\r\n");
        if (trim_start == std::string_view::npos) {
            error("Expected MEASUREMENT option value");
            return StringPool::INVALID_ID;
        }
        size_t trim_end = text.find_last_not_of(" \t\r\n");
        return stringPool().intern(text.substr(trim_start, trim_end - trim_start + 1));
    };

    auto parse_measurement_clause = [&]() {
        stmt->has_measurement = true;
        stmt->drop_measurement = false;
        stmt->measurement_options.clear();
        expect(TokenType::LEFT_PAREN, "Expected '(' after MEASUREMENT");
        if (check(TokenType::RIGHT_PAREN)) {
            error("MEASUREMENT clause must include at least one option");
        }
        while (!check(TokenType::RIGHT_PAREN) && !check(TokenType::END_OF_FILE)) {
            auto key = parse_measurement_key();
            expect(TokenType::EQUAL, "Expected '=' after MEASUREMENT option key");
            auto value = parse_measurement_value();
            stmt->measurement_options.emplace_back(key, value);
            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after MEASUREMENT options");
        if (stmt->measurement_options.empty()) {
            error("MEASUREMENT clause must include at least one option");
        }
    };

    auto parse_job_body = [&]() {
        if (match(TokenType::KW_AS) || matchContextual("AS")) {
            stmt->has_job_body = true;
            stmt->job_type = JobType::SQL;
            if (matchContextual("SQL")) {
                // Optional SQL keyword in canonical ALTER JOB body form.
            }
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected SQL string after AS");
            } else {
                stmt->job_sql = current().value.string_id;
                advance();
            }
            return true;
        }
        if (match(TokenType::KW_CALL) || matchContextual("CALL")) {
            stmt->has_job_body = true;
            stmt->job_type = JobType::PROCEDURE;
            SchemaPath proc_path = parseSchemaPath(state_);
            if (proc_path.isEmpty()) {
                error("Expected procedure name after CALL");
            } else {
                std::string proc_name = schemaPathToString(proc_path, stringPool());
                stmt->procedure_name = stringPool().intern(proc_name);
            }
            if (match(TokenType::LEFT_PAREN)) {
                expect(TokenType::RIGHT_PAREN, "Expected ')' after CALL procedure");
            }
            return true;
        }
        if (match(TokenType::KW_EXECUTE) || matchContextual("EXEC")) {
            stmt->has_job_body = true;
            stmt->job_type = JobType::EXTERNAL;
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected command string after EXEC");
            } else {
                stmt->external_command = current().value.string_id;
                advance();
            }
            return true;
        }
        return false;
    };

    while (!check(TokenType::SEMICOLON) && !check(TokenType::END_OF_FILE)) {
        bool saw_set = match(TokenType::KW_SET);

        if (saw_set && matchContextual("SECRET")) {
            stmt->has_secret = true;
            stmt->drop_secret = false;
            stmt->secret_key = expectIdentifier("Expected secret key after SECRET");
            expect(TokenType::EQUAL, "Expected '=' after secret key");
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal for secret value");
            } else {
                stmt->secret_value = current().value.string_id;
                advance();
            }
        } else if (saw_set && matchContextual("MEASUREMENT")) {
            parse_measurement_clause();
        } else if (match(TokenType::KW_DROP) || matchContextual("DROP")) {
            if (matchContextual("MEASUREMENT")) {
                stmt->has_measurement = true;
                stmt->drop_measurement = true;
                stmt->measurement_options.clear();
            } else if (matchContextual("SECRET")) {
                stmt->has_secret = true;
                stmt->drop_secret = true;
                stmt->secret_key = expectIdentifier("Expected secret key after DROP SECRET");
            } else {
                error("Expected MEASUREMENT or SECRET after DROP");
            }
        } else if (parse_schedule()) {
            // handled
        } else if (parse_job_body()) {
            // handled
        } else if (matchContextual("DEPENDS")) {
            if (!(match(TokenType::KW_ON) || matchContextual("ON"))) {
                error("Expected ON after DEPENDS");
            }
            stmt->has_depends_on = true;
            if (matchContextual("NONE")) {
                stmt->clear_depends_on = true;
            } else {
                do {
                    stmt->depends_on.push_back(expectIdentifier("Expected dependency job name"));
                } while (match(TokenType::COMMA));
            }
        } else if (matchContextual("CLASS")) {
            expect(TokenType::EQUAL, "Expected '=' after CLASS");
            stmt->has_job_class = true;
            stmt->job_class = expectIdentifier("Expected job class");
        } else if (parse_partition()) {
            // handled
        } else if (match(TokenType::KW_ON) || matchContextual("ON")) {
            if (!matchContextual("COMPLETION")) {
                error("Expected COMPLETION after ON");
            }
            stmt->has_on_completion = true;
            if (matchContextual("PRESERVE")) {
                stmt->on_completion = JobOnCompletion::PRESERVE;
            } else if (match(TokenType::KW_DROP) || matchContextual("DROP")) {
                stmt->on_completion = JobOnCompletion::DROP;
            } else {
                error("Expected PRESERVE or DROP after ON COMPLETION");
            }
        } else if (matchContextual("STATE")) {
            expect(TokenType::EQUAL, "Expected '=' after STATE");
            stmt->has_state = true;
            if (matchContextual("ENABLED")) {
                stmt->state = JobState::ENABLED;
            } else if (matchContextual("DISABLED")) {
                stmt->state = JobState::DISABLED;
            } else if (matchContextual("PAUSED")) {
                stmt->state = JobState::PAUSED;
            } else {
                error("Expected ENABLED, DISABLED, or PAUSED after STATE");
            }
        } else if (matchContextual("MAX_RETRIES")) {
            expect(TokenType::EQUAL, "Expected '=' after MAX_RETRIES");
            if (!check(TokenType::INTEGER_LITERAL)) {
                error("Expected integer value for MAX_RETRIES");
            } else {
                stmt->has_max_retries = true;
                stmt->max_retries = static_cast<uint32_t>(current().value.int_value);
                advance();
            }
        } else if (matchContextual("RETRY_BACKOFF")) {
            expect(TokenType::EQUAL, "Expected '=' after RETRY_BACKOFF");
            stmt->has_retry_backoff = true;
            stmt->retry_backoff_seconds = parse_duration_seconds("RETRY_BACKOFF");
        } else if (matchContextual("TIMEOUT")) {
            expect(TokenType::EQUAL, "Expected '=' after TIMEOUT");
            stmt->has_timeout = true;
            stmt->timeout_seconds = parse_duration_seconds("TIMEOUT");
        } else if (matchContextual("RUN")) {
            if (!(match(TokenType::KW_AS) || matchContextual("AS"))) {
                error("Expected AS after RUN");
            }
            stmt->has_run_as = true;
            stmt->run_as_role = expectIdentifier("Expected role name after RUN AS");
        } else if (matchContextual("DESCRIPTION")) {
            expect(TokenType::EQUAL, "Expected '=' after DESCRIPTION");
            stmt->has_description = true;
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal for DESCRIPTION");
            } else {
                stmt->description = current().value.string_id;
                advance();
            }
        } else {
            if (saw_set) {
                error("Expected SCHEDULE, MEASUREMENT, STATE, MAX_RETRIES, RETRY_BACKOFF, TIMEOUT, RUN AS, DESCRIPTION, AS, CALL, EXEC, DEPENDS ON, PARTITION BY, ON COMPLETION, or CLASS after SET");
            }
            break;
        }

        if (!match(TokenType::COMMA)) {
            // Continue if next clause is another SET without comma
            continue;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

AlterSystemStmt* Parser::parseAlterSystem() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    if (!(match(TokenType::KW_SET) || matchContextual("SET"))) {
        error("Expected SET after ALTER SYSTEM");
    }

    auto parseConfigKey = [&]() -> StringPool::StringId {
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            return id;
        }
        if (!isIdentifier()) {
            error("Expected configuration key");
            return StringPool::INVALID_ID;
        }

        std::string name = std::string(stringPool().get(current().value.string_id));
        advance();
        while (match(TokenType::DOT)) {
            if (!isIdentifier()) {
                error("Expected configuration key segment after '.'");
                break;
            }
            name += ".";
            name += std::string(stringPool().get(current().value.string_id));
            advance();
        }

        return stringPool().intern(name);
    };

    stmt->name = parseConfigKey();
    expect(TokenType::EQUAL, "Expected '=' after configuration key");
    stmt->value = parseExpression();

    stmt->span = makeSpan(start);
    return stmt;
}

AlterTableStmt* Parser::parseAlterTable() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<AlterTableStmt>();

    // IF EXISTS (IF, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    // ONLY
    if (checkContextual("ONLY")) {
        matchContextual("ONLY");
        stmt->only = true;
    }

    // Table name
    stmt->table_path = parseSchemaPath(state_);

    // Action
    if (matchContextual("ADD")) {
        if (matchContextual("COLUMN")) {
            stmt->action = AlterTableAction::ADD_COLUMN;
            stmt->column = parseColumnDef();
        } else if (matchContextual("CONSTRAINT")) {
            stmt->action = AlterTableAction::ADD_CONSTRAINT;
            // Parse constraint inline (don't re-read CONSTRAINT keyword)
            auto* constraint = arena_.create<TableConstraint>();
            constraint->name = expectIdentifier("Expected constraint name");

            // Constraint type
            if (matchContextual("PRIMARY")) {
                expectContextual("KEY", "Expected KEY after PRIMARY");
                parsePrimaryKeyConstraint(constraint);
            } else if (matchContextual("UNIQUE")) {
                parseUniqueConstraint(constraint);
            } else if (matchContextual("FOREIGN")) {
                expectContextual("KEY", "Expected KEY after FOREIGN");
                parseForeignKeyConstraint(constraint);
            } else if (matchContextual("CHECK")) {
                parseCheckConstraint(constraint);
            }
            stmt->constraint = constraint;
        } else {
            // Assume ADD COLUMN without COLUMN keyword
            stmt->action = AlterTableAction::ADD_COLUMN;
            stmt->column = parseColumnDef();
        }
    } else if (match(TokenType::KW_DROP)) {
        // DROP is a Gatekeeper keyword
        if (matchContextual("COLUMN")) {
            stmt->action = AlterTableAction::DROP_COLUMN;
            stmt->column_name = expectIdentifier("Expected column name");
            if (matchContextual("CASCADE")) stmt->cascade = true;
        } else if (matchContextual("CONSTRAINT")) {
            stmt->action = AlterTableAction::DROP_CONSTRAINT;
            stmt->constraint_name = expectIdentifier("Expected constraint name");
            if (matchContextual("CASCADE")) stmt->cascade = true;
        } else {
            // DROP without COLUMN/CONSTRAINT - assume DROP COLUMN
            stmt->action = AlterTableAction::DROP_COLUMN;
            stmt->column_name = expectIdentifier("Expected column name");
            if (matchContextual("CASCADE")) stmt->cascade = true;
        }
    } else if (match(TokenType::KW_ALTER) || matchContextual("ALTER")) {
        if (matchContextual("COLUMN")) {
            stmt->column_name = expectIdentifier("Expected column name");
        } else {
            stmt->column_name = expectIdentifier("Expected column name");
        }

        bool handled = false;
        if (matchContextual("POSITION")) {
            if (!check(TokenType::INTEGER_LITERAL)) {
                error("ALTER TABLE ALTER COLUMN POSITION requires an integer literal");
            } else {
                stmt->action = AlterTableAction::ALTER_COLUMN_POSITION;
                stmt->position_1_based = static_cast<int32_t>(current().value.int_value);
                stmt->has_position = true;
                advance();
            }
            handled = true;
        } else if (match(TokenType::KW_SET) || matchContextual("SET")) {
            if (matchContextual("STATISTICS")) {
                stmt->action = AlterTableAction::SET_STATISTICS;
                if (!check(TokenType::INTEGER_LITERAL)) {
                    error("Expected integer after SET STATISTICS");
                } else {
                    stmt->statistics_target = static_cast<int32_t>(current().value.int_value);
                    stmt->has_statistics_target = true;
                    advance();
                }
                handled = true;
            } else if (matchContextual("STORAGE")) {
                stmt->action = AlterTableAction::SET_STORAGE;
                if (!isIdentifier()) {
                    error("Expected storage type after SET STORAGE");
                } else {
                    stmt->storage_type = currentIdentifier();
                    advance();
                }
                handled = true;
            } else if (matchContextual("DATA")) {
                expectContextual("TYPE", "Expected TYPE after SET DATA");
                stmt->action = AlterTableAction::ALTER_COLUMN;
                auto* col = arena_.create<ColumnDef>();
                col->name = stmt->column_name;
                col->type = parseTypeName();
                stmt->column = col;
                handled = true;
            } else if (match(TokenType::KW_DEFAULT) || matchContextual("DEFAULT")) {
                stmt->action = AlterTableAction::ALTER_COLUMN_SET_DEFAULT;
                stmt->default_expr = parseExpression();
                stmt->has_default_expr = (stmt->default_expr != nullptr);
                handled = true;
            } else if (match(TokenType::KW_NOT) || matchContextual("NOT")) {
                if (match(TokenType::KW_NULL) || matchContextual("NULL")) {
                    // ok
                } else {
                    error("Expected NULL after SET NOT");
                }
                stmt->action = AlterTableAction::ALTER_COLUMN_SET_NOT_NULL;
                handled = true;
            }
        } else if (match(TokenType::KW_DROP) || matchContextual("DROP")) {
            if (match(TokenType::KW_DEFAULT) || matchContextual("DEFAULT")) {
                stmt->action = AlterTableAction::ALTER_COLUMN_DROP_DEFAULT;
                handled = true;
            } else if (match(TokenType::KW_NOT) || matchContextual("NOT")) {
                if (match(TokenType::KW_NULL) || matchContextual("NULL")) {
                    // ok
                } else {
                    error("Expected NULL after DROP NOT");
                }
                stmt->action = AlterTableAction::ALTER_COLUMN_DROP_NOT_NULL;
                handled = true;
            }
        } else if (matchContextual("TYPE")) {
            stmt->action = AlterTableAction::ALTER_COLUMN;
            auto* col = arena_.create<ColumnDef>();
            col->name = stmt->column_name;
            col->type = parseTypeName();
            stmt->column = col;
            handled = true;
        }
        if (!handled) {
            error("Expected POSITION, SET/DROP DEFAULT, SET/DROP NOT NULL, SET STATISTICS, SET STORAGE, SET DATA TYPE, or TYPE after ALTER COLUMN");
        }
    } else if (matchContextual("ATTACH")) {
        expectContextual("PARTITION", "Expected PARTITION after ATTACH");
        stmt->action = AlterTableAction::ATTACH_PARTITION;
        stmt->partition_path = parseSchemaPath(state_);
        if (matchContextual("FOR")) {
            size_t bounds_start = previous().span.start.offset;
            expectContextual("VALUES", "Expected VALUES after FOR");

            while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
                advance();
            }
            if (previous().span.length > 0 && bounds_start < lexer_.input().size()) {
                size_t bounds_end = previous().span.start.offset + previous().span.length;
                if (bounds_end > bounds_start) {
                    std::string_view input = lexer_.input();
                    std::string bounds(input.substr(bounds_start, bounds_end - bounds_start));
                    stmt->partition_bounds = stringPool().intern(bounds);
                    stmt->has_partition_bounds = true;
                }
            }
        }
    } else if (matchContextual("DETACH")) {
        expectContextual("PARTITION", "Expected PARTITION after DETACH");
        stmt->action = AlterTableAction::DETACH_PARTITION;
        stmt->partition_path = parseSchemaPath(state_);
    } else if (matchContextual("RENAME")) {
        if (matchContextual("COLUMN")) {
            stmt->action = AlterTableAction::RENAME_COLUMN;
            stmt->column_name = expectIdentifier("Expected column name");
            expectContextual("TO", "Expected TO after column name");
            stmt->new_name = expectIdentifier("Expected new column name");
        } else if (matchContextual("CONSTRAINT")) {
            stmt->action = AlterTableAction::RENAME_CONSTRAINT;
            stmt->constraint_name = expectIdentifier("Expected constraint name");
            expectContextual("TO", "Expected TO after constraint name");
            stmt->new_name = expectIdentifier("Expected new constraint name");
        } else if (matchContextual("TO")) {
            stmt->action = AlterTableAction::RENAME_TABLE;
            stmt->new_name = expectIdentifier("Expected new table name");
        }
    } else if (match(TokenType::KW_SET)) {
        if (matchContextual("TABLESPACE")) {
            stmt->action = AlterTableAction::SET_TABLESPACE;
            stmt->tablespace = parseSchemaPath(state_);
            if (!stmt->tablespace.isEmpty() && stmt->tablespace.components.size() != 1) {
                errorCode("PRS_0505",
                          "TABLESPACE names are database-scoped and must not be schema-qualified");
            }
        } else if (matchContextual("SCHEMA")) {
            stmt->action = AlterTableAction::SET_SCHEMA;
            stmt->target_schema = parseSchemaPath(state_);
        }
    } else if (matchContextual("INHERIT")) {
        stmt->action = AlterTableAction::INHERIT;
        stmt->inherit_parent = parseSchemaPath(state_);
        stmt->has_inherit_parent = !stmt->inherit_parent.isEmpty();
    } else if (matchContextual("NO")) {
        if (matchContextual("INHERIT")) {
            stmt->action = AlterTableAction::NO_INHERIT;
            stmt->inherit_parent = parseSchemaPath(state_);
            stmt->has_inherit_parent = !stmt->inherit_parent.isEmpty();
        }
    } else if (matchContextual("ENABLE")) {
        if (matchContextual("TRIGGER")) {
            stmt->action = AlterTableAction::ENABLE_TRIGGER;
            if (matchContextual("ALL")) {
                stmt->trigger_all = true;
            } else {
                stmt->trigger_name = expectIdentifier("Expected trigger name");
            }
        } else {
            expectContextual("ROW", "Expected ROW after ENABLE");
            expectContextual("LEVEL", "Expected LEVEL after ROW");
            expectContextual("SECURITY", "Expected SECURITY after LEVEL");
            stmt->action = AlterTableAction::ENABLE_RLS;
        }
    } else if (matchContextual("DISABLE")) {
        if (matchContextual("TRIGGER")) {
            stmt->action = AlterTableAction::DISABLE_TRIGGER;
            if (matchContextual("ALL")) {
                stmt->trigger_all = true;
            } else {
                stmt->trigger_name = expectIdentifier("Expected trigger name");
            }
        } else {
            expectContextual("ROW", "Expected ROW after DISABLE");
            expectContextual("LEVEL", "Expected LEVEL after ROW");
            expectContextual("SECURITY", "Expected SECURITY after LEVEL");
            stmt->action = AlterTableAction::DISABLE_RLS;
        }
    } else if (matchContextual("FORCE")) {
        expectContextual("ROW", "Expected ROW after FORCE");
        expectContextual("LEVEL", "Expected LEVEL after ROW");
        expectContextual("SECURITY", "Expected SECURITY after LEVEL");
        stmt->action = AlterTableAction::FORCE_RLS;
    } else if (matchContextual("NO")) {
        if (matchContextual("FORCE")) {
            expectContextual("ROW", "Expected ROW after NO FORCE");
            expectContextual("LEVEL", "Expected LEVEL after ROW");
            expectContextual("SECURITY", "Expected SECURITY after LEVEL");
            stmt->action = AlterTableAction::NO_FORCE_RLS;
        }
    } else if (matchContextual("VALIDATE")) {
        stmt->action = AlterTableAction::VALIDATE_CONSTRAINT;
        matchContextual("CONSTRAINT");
        stmt->constraint_name = expectIdentifier("Expected constraint name");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// DROP Statements
// =============================================================================

Statement* Parser::parseDrop() {
    ParseModeGuard guard(state_, ParseMode::DDL);

    if (matchContextual("SCHEDULE")) {
        errorCode("PRS_0505",
                  "Top-level DROP SCHEDULE is not supported in v3; use ALTER/DROP JOB ...");
        return nullptr;
    }
    if (matchContextual("SEARCH")) {
        if (matchContextual("INDEX")) {
            errorCode("PRS_0505",
                      "DROP SEARCH INDEX is not supported in v3; use DROP INDEX ...");
        } else {
            errorCode("PRS_0505", "Unsupported SEARCH drop surface");
        }
        return nullptr;
    }
    if (matchContextual("VECTOR")) {
        if (matchContextual("INDEX")) {
            errorCode("PRS_0505",
                      "DROP VECTOR INDEX is not supported in v3; use DROP INDEX ...");
        } else {
            errorCode("PRS_0505", "Unsupported VECTOR drop surface");
        }
        return nullptr;
    }
    if (matchContextual("CONNECTION")) {
        if (!matchContextual("RULE")) {
            errorCode("PRS_0505", "Expected RULE after DROP CONNECTION");
            return nullptr;
        }
        if (!requireFeature(kFeatureSecurityConnectionRuleDdl)) {
            return nullptr;
        }
        return parseDropConnectionRule();
    }
    if (matchContextual("TOKEN")) {
        if (!requireFeature(kFeatureSecurityTokenDdl)) {
            return nullptr;
        }
        return parseDropToken();
    }
    if (matchContextual("QUOTA")) {
        if (!matchContextual("PROFILE")) {
            errorCode("PRS_0505", "Expected PROFILE after DROP QUOTA");
            return nullptr;
        }
        if (!requireFeature(kFeatureSecurityQuotaProfileDdl)) {
            return nullptr;
        }
        return parseDropQuotaProfile();
    }
    if (matchContextual("EXTENSION")) {
        return parseDropExtension();
    }
    if (matchContextual("REPLICATION")) {
        if (!matchContextual("CHANNEL")) {
            errorCode("PRS_0505", "Expected CHANNEL after DROP REPLICATION");
            return nullptr;
        }
        return parseDropReplicationChannel();
    }
    if (matchContextual("PUBLICATION")) {
        return parseDropPublication();
    }
    if (matchContextual("SUBSCRIPTION")) {
        return parseDropSubscription();
    }
    if (matchContextual("CDC")) {
        if (!matchContextual("TABLE")) {
            errorCode("PRS_0505", "Expected TABLE after DROP CDC");
            return nullptr;
        }
        return parseDropCdcTable();
    }
    if (matchContextual("CLUSTER")) {
        return parseDropClusterControl();
    }
    if (matchContextual("CUBE")) {
        return parseDropCubeControl();
    }
    if (matchContextual("COMMENT")) {
        return parseDropComment();
    }

    if (matchContextual("SCHEMA")) return parseDropSchema();
    if (matchContextual("DATABASE")) {
        if (matchContextual("CONNECTION")) return parseDropDatabaseConnection();
        return parseDropDatabase();
    }
    if (matchContextual("TABLESPACE")) return parseDropTablespace();
    if (matchContextual("TABLE")) return parseDropTable();
    if (matchContextual("INDEX")) return parseDropIndex();
    if (matchContextual("VIEW")) return parseDropView();
    if (matchContextual("JOB")) return parseDropJob();
    if (matchContextual("DOMAIN")) return parseDropDomain();
    if (matchContextual("TYPE")) return parseDropType();
    if (matchContextual("FUNCTION")) return parseDropFunction();
    if (matchContextual("PROCEDURE")) return parseDropProcedure();
    if (matchContextual("TRIGGER")) return parseDropTrigger();
    if (matchContextual("PACKAGE")) return parseDropPackage();
    if (matchContextual("ROLE")) return parseDropRole();
    if (matchContextual("GROUP")) return parseDropGroup();
    if (matchContextual("POLICY")) {
        if (!requireFeature(kFeatureSecurityModelPolicyDdl)) {
            return nullptr;
        }
        return parseDropPolicy();
    }
    if (matchContextual("EXCEPTION")) return parseDropException();
    if (matchContextual("SEQUENCE")) return parseDropSequence();
    if (matchContextual("PUBLIC")) {
        if (matchContextual("SYNONYM")) {
            auto* stmt = parseDropSynonym();
            if (stmt) {
                stmt->is_public = true;
            }
            return stmt;
        }
        errorCode("PRS_0505", "Expected SYNONYM after DROP PUBLIC");
        return nullptr;
    }
    if (matchContextual("SYNONYM")) return parseDropSynonym();
    if (matchContextual("UDR")) return parseDropUdr();
    if (matchContextual("SERVER")) return parseDropForeignServer();
    if (matchContextual("USER")) {
        if (matchContextual("MAPPING")) {
            return parseDropUserMapping();
        }
        if (!requireFeature(kFeatureSecurityUserAccountDdl)) {
            return nullptr;
        }
        return parseDropUser();
    }
    if (matchContextual("FOREIGN")) {
        expectContextual("TABLE", "Expected TABLE after FOREIGN");
        return parseDropForeignTable();
    }
    if (matchContextual("MATERIALIZED")) {
        errorCode("PRS_0505",
                  "DROP MATERIALIZED VIEW is not supported in v3; use DROP VIEW");
        return nullptr;
    }

    error("Expected object type after DROP");
    return nullptr;
}

Statement* Parser::parseDropSchedule() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<DropJobStmt>();
    stmt->job_name = expectIdentifier("Expected schedule name");
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseDropConnectionRule() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();
    auto match_word = [&](TokenType token, const char* word) {
        return match(token) || matchContextual(word);
    };
    StringPool::StringId rule_name = expectIdentifier("Expected connection rule name");
    std::string rule = std::string(stringPool().get(rule_name));
    if (!matchContextual("EXPECT") ||
        !matchContextual("VERSION") ||
        !check(TokenType::INTEGER_LITERAL)) {
        errorCode("SEC_1237", "DROP CONNECTION RULE requires EXPECT VERSION <uint64>");
    } else {
        advance();
    }
    stmt->name = stringPool().intern("security.connection_rule.drop." + rule);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern("");
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseDropToken() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();
    StringPool::StringId token_name = expectIdentifier("Expected token name");
    std::string token = std::string(stringPool().get(token_name));
    stmt->name = stringPool().intern("security.token.drop." + token);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern("");
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseDropQuotaProfile() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();
    StringPool::StringId profile_name = expectIdentifier("Expected quota profile name");
    std::string profile = std::string(stringPool().get(profile_name));
    stmt->name = stringPool().intern("security.quota_profile.drop." + profile);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern("");
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseDropExtension() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    bool if_exists = false;
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        if_exists = true;
    }

    StringPool::StringId extension_name = expectIdentifier("Expected extension name");
    std::string extension = std::string(stringPool().get(extension_name));
    std::string payload;
    if (if_exists) {
        payload = "IF_EXISTS=1";
    }
    if (matchContextual("CASCADE")) {
        if (!payload.empty()) payload.push_back(';');
        payload.append("CASCADE=1");
    } else if (matchContextual("RESTRICT")) {
        if (!payload.empty()) payload.push_back(';');
        payload.append("RESTRICT=1");
    }

    stmt->name = stringPool().intern("platform.extension.drop." + extension);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseDropReplicationChannel() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    bool if_exists = false;
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        if_exists = true;
    }

    StringPool::StringId channel_name = expectIdentifier("Expected replication channel name");
    std::string channel = std::string(stringPool().get(channel_name));
    std::string payload;
    if (if_exists) {
        payload = "IF_EXISTS=1";
    }
    if (matchContextual("CASCADE")) {
        if (!payload.empty()) payload.push_back(';');
        payload.append("CASCADE=1");
    } else if (matchContextual("RESTRICT")) {
        if (!payload.empty()) payload.push_back(';');
        payload.append("RESTRICT=1");
    }

    stmt->name = stringPool().intern("replication.channel.drop." + channel);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseDropPublication() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    bool if_exists = false;
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        if_exists = true;
    }

    StringPool::StringId publication_name = expectIdentifier("Expected publication name");
    std::string publication = std::string(stringPool().get(publication_name));
    std::string payload;
    if (if_exists) {
        payload = "IF_EXISTS=1";
    }
    if (matchContextual("CASCADE")) {
        if (!payload.empty()) payload.push_back(';');
        payload.append("CASCADE=1");
    } else if (matchContextual("RESTRICT")) {
        if (!payload.empty()) payload.push_back(';');
        payload.append("RESTRICT=1");
    }

    stmt->name = stringPool().intern("replication.publication.drop." + publication);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseDropSubscription() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    bool if_exists = false;
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        if_exists = true;
    }

    StringPool::StringId subscription_name = expectIdentifier("Expected subscription name");
    std::string subscription = std::string(stringPool().get(subscription_name));
    std::string payload;
    if (if_exists) {
        payload = "IF_EXISTS=1";
    }

    stmt->name = stringPool().intern("replication.subscription.drop." + subscription);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseDropCdcTable() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    bool if_exists = false;
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        if_exists = true;
    }

    SchemaPath table_path = parseSchemaPath(state_);
    if (table_path.isEmpty()) {
        errorCode("PRS_0504", "Expected CDC table name");
    }

    std::string table_name = schemaPathToString(table_path, stringPool());
    std::string payload;
    if (if_exists) {
        payload = "IF_EXISTS=1";
    }
    if (matchContextual("CASCADE")) {
        if (!payload.empty()) payload.push_back(';');
        payload.append("CASCADE=1");
    } else if (matchContextual("RESTRICT")) {
        if (!payload.empty()) payload.push_back(';');
        payload.append("RESTRICT=1");
    }

    stmt->name = stringPool().intern("etl.cdc.table.drop." + table_name);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseDropDatabaseConnection() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    bool if_exists = false;
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        if_exists = true;
    }

    StringPool::StringId connection_name = expectIdentifier("Expected database connection name");
    std::string connection = std::string(stringPool().get(connection_name));
    std::string payload;
    if (if_exists) {
        payload = "IF_EXISTS=1";
    }

    stmt->name = stringPool().intern("external.database_connection.drop." + connection);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// DROP SCHEMA
// =============================================================================

DropSchemaStmt* Parser::parseDropSchema() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropSchemaStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->schemas.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    if (matchContextual("CASCADE")) stmt->cascade = true;
    else if (matchContextual("RESTRICT")) stmt->restrict = true;

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// DROP DATABASE
// =============================================================================

DropDatabaseStmt* Parser::parseDropDatabase() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropDatabaseStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    stmt->database_path = parseSchemaPath(state_);
    if (stmt->database_path.isEmpty()) {
        error("Expected database name");
    }

    if (matchContextual("CASCADE") || matchContextual("FORCE")) {
        stmt->force = true;
    } else if (matchContextual("RESTRICT")) {
        stmt->force = false;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// DROP DOMAIN
// =============================================================================

DropDomainStmt* Parser::parseDropDomain() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropDomainStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        SchemaPath domain_path = parseSchemaPath(state_);
        if (!domain_path.isEmpty() && domain_path.components.size() != 1) {
            errorCode("PRS_0505",
                      "DOMAIN names are global and must not be schema-qualified");
        }
        stmt->domains.push_back(std::move(domain_path));
    } while (match(TokenType::COMMA));

    if (matchContextual("CASCADE")) {
        error("DROP DOMAIN does not support CASCADE");
    } else if (matchContextual("RESTRICT")) {
        stmt->restrict = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// DROP JOB
// =============================================================================

DropJobStmt* Parser::parseDropJob() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropJobStmt>();
    stmt->job_name = expectIdentifier("Expected job name");

    if (matchContextual("KEEP")) {
        expectContextual("HISTORY", "Expected HISTORY after KEEP");
        stmt->keep_history = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

DropTableStmt* Parser::parseDropTable() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropTableStmt>();

    // IF EXISTS (IF, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    // Table names
    do {
        stmt->tables.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    // CASCADE / RESTRICT
    if (matchContextual("CASCADE")) stmt->cascade = true;
    else if (matchContextual("RESTRICT")) stmt->restrict = true;

    stmt->span = makeSpan(start);
    return stmt;
}

DropIndexStmt* Parser::parseDropIndex() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropIndexStmt>();

    // CONCURRENTLY
    if (matchContextual("CONCURRENTLY")) {
        stmt->concurrent = true;
    }

    // IF EXISTS (IF, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    // Index names
    do {
        SchemaPath index_path = parseSchemaPath(state_);
        if (!index_path.isEmpty() && index_path.components.size() < 2) {
            errorCode("PRS_0505",
                      "DROP INDEX requires explicit parent-qualified index reference");
        }
        stmt->indexes.push_back(std::move(index_path));
    } while (match(TokenType::COMMA));

    // CASCADE
    if (matchContextual("CASCADE")) stmt->cascade = true;

    stmt->span = makeSpan(start);
    return stmt;
}

DropViewStmt* Parser::parseDropView() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropViewStmt>();

    // IF EXISTS (IF, EXISTS are Gatekeeper keywords)
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    // View names
    do {
        stmt->views.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    // CASCADE
    if (matchContextual("CASCADE")) stmt->cascade = true;

    stmt->span = makeSpan(start);
    return stmt;
}

DropFunctionStmt* Parser::parseDropFunction() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropFunctionStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->functions.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    stmt->span = makeSpan(start);
    return stmt;
}

DropProcedureStmt* Parser::parseDropProcedure() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropProcedureStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->procedures.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    stmt->span = makeSpan(start);
    return stmt;
}

DropTriggerStmt* Parser::parseDropTrigger() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropTriggerStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        SchemaPath trigger_path = parseSchemaPath(state_);
        if (!trigger_path.isEmpty() && trigger_path.components.size() < 2) {
            errorCode("PRS_0505",
                      "DROP TRIGGER requires explicit parent-qualified trigger reference");
        }
        stmt->triggers.push_back(std::move(trigger_path));
    } while (match(TokenType::COMMA));

    stmt->span = makeSpan(start);
    return stmt;
}

DropPackageStmt* Parser::parseDropPackage() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropPackageStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->packages.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    stmt->span = makeSpan(start);
    return stmt;
}

DropRoleStmt* Parser::parseDropRole() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropRoleStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->roles.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    if (matchContextual("CASCADE")) {
        stmt->cascade = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

DropUserStmt* Parser::parseDropUser() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropUserStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->users.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    if (stmt->users.empty() || stmt->users.front().isEmpty()) {
        error("Expected user name");
    }

    if (matchContextual("CASCADE")) {
        stmt->cascade = true;
    } else if (matchContextual("RESTRICT")) {
        stmt->cascade = false;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

DropGroupStmt* Parser::parseDropGroup() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropGroupStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->groups.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    if (matchContextual("CASCADE")) {
        stmt->cascade = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

DropPolicyStmt* Parser::parseDropPolicy() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropPolicyStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    stmt->policy_name = expectIdentifier("Expected policy name");

    expect(TokenType::KW_ON, "Expected ON after policy name");
    stmt->table_path = parseSchemaPath(state_);

    stmt->span = makeSpan(start);
    return stmt;
}

DropExceptionStmt* Parser::parseDropException() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropExceptionStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->exceptions.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    stmt->span = makeSpan(start);
    return stmt;
}

DropForeignServerStmt* Parser::parseDropForeignServer() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropForeignServerStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    stmt->server_name = expectIdentifier("Expected server name");

    if (matchContextual("CASCADE")) {
        stmt->cascade = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

DropForeignTableStmt* Parser::parseDropForeignTable() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropForeignTableStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->tables.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    if (matchContextual("CASCADE")) {
        stmt->cascade = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

DropUserMappingStmt* Parser::parseDropUserMapping() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropUserMappingStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    expectContextual("FOR", "Expected FOR after DROP USER MAPPING");

    if (matchContextual("CURRENT_USER")) {
        stmt->target = UserMappingTarget::CURRENT_USER;
    } else if (matchContextual("CURRENT")) {
        expectContextual("USER", "Expected USER after CURRENT");
        stmt->target = UserMappingTarget::CURRENT_USER;
    } else if (matchContextual("SESSION_USER")) {
        stmt->target = UserMappingTarget::SESSION_USER;
    } else if (matchContextual("SESSION")) {
        expectContextual("USER", "Expected USER after SESSION");
        stmt->target = UserMappingTarget::SESSION_USER;
    } else if (matchContextual("PUBLIC")) {
        stmt->target = UserMappingTarget::PUBLIC_ROLE;
    } else if (matchContextual("USER")) {
        if (isIdentifier()) {
            stmt->target = UserMappingTarget::USER_NAME;
            stmt->user_name = expectIdentifier("Expected user name");
        } else {
            stmt->target = UserMappingTarget::CURRENT_USER;
        }
    } else {
        stmt->target = UserMappingTarget::USER_NAME;
        stmt->user_name = expectIdentifier("Expected user name");
    }

    expectContextual("SERVER", "Expected SERVER after user mapping target");
    stmt->server_name = expectIdentifier("Expected server name");

    stmt->span = makeSpan(start);
    return stmt;
}

DropSynonymStmt* Parser::parseDropSynonym() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropSynonymStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->synonyms.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    if (matchContextual("CASCADE")) {
        stmt->cascade = true;
    } else if (matchContextual("RESTRICT")) {
        stmt->restrict = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

DropUdrStmt* Parser::parseDropUdr() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropUdrStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->udrs.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    if (matchContextual("CASCADE")) {
        stmt->cascade = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

DropTypeStmt* Parser::parseDropType() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropTypeStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->types.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    if (matchContextual("CASCADE")) {
        stmt->cascade = true;
    } else if (matchContextual("RESTRICT")) {
        stmt->restrict = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

DropSequenceStmt* Parser::parseDropSequence() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<DropSequenceStmt>();

    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        stmt->if_exists = true;
    }

    do {
        stmt->sequences.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    if (matchContextual("CASCADE")) {
        stmt->cascade = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// TRUNCATE Statement
// =============================================================================

Statement* Parser::parseTruncate() {
    ParseModeGuard guard(state_, ParseMode::DDL);

    if (matchContextual("TABLE")) {
        return parseTruncateTable();
    }

    // TRUNCATE without TABLE keyword
    return parseTruncateTable();
}

TruncateTableStmt* Parser::parseTruncateTable() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<TruncateTableStmt>();

    // Table names
    do {
        stmt->tables.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    // Options
    while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
        if (matchContextual("RESTART")) {
            expectContextual("IDENTITY", "Expected IDENTITY after RESTART");
            stmt->restart_identity = true;
        } else if (matchContextual("CONTINUE")) {
            expectContextual("IDENTITY", "Expected IDENTITY after CONTINUE");
            stmt->continue_identity = true;
        } else if (matchContextual("CASCADE")) {
            stmt->cascade = true;
        } else {
            break;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// WITH Clause
// =============================================================================

Statement* Parser::parseWithStatement() {
    WithClause* with = parseWithClause();
    if (!with) {
        return nullptr;
    }

    if (match(TokenType::KW_SELECT)) {
        auto* stmt = parseSelect();
        stmt->with = with;
        return stmt;
    }
    if (match(TokenType::KW_INSERT)) {
        if (!requireFeature(kFeatureDmlWritableCte)) return nullptr;
        auto* stmt = parseInsert();
        stmt->with = with;
        return stmt;
    }
    if (match(TokenType::KW_UPDATE)) {
        if (!requireFeature(kFeatureDmlWritableCte)) return nullptr;
        auto* stmt = parseUpdate();
        stmt->with = with;
        return stmt;
    }
    if (match(TokenType::KW_DELETE)) {
        if (!requireFeature(kFeatureDmlWritableCte)) return nullptr;
        auto* stmt = parseDelete();
        stmt->with = with;
        return stmt;
    }

    error("Expected SELECT, INSERT, UPDATE, or DELETE after WITH clause");
    return nullptr;
}

WithClause* Parser::parseWithClause() {
    ParseModeGuard guard(state_, ParseMode::WITH_CLAUSE);

    if (!match(TokenType::KW_WITH)) {
        error("Expected WITH");
        return nullptr;
    }

    auto* with = arena_.create<WithClause>();

    if (matchContextual("RECURSIVE")) {
        with->recursive = true;
    }

    do {
        CTE cte;
        cte.name = expectIdentifier("Expected CTE name");

        if (match(TokenType::LEFT_PAREN)) {
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    cte.column_names.push_back(expectIdentifier("Expected CTE column name"));
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after CTE column list");
        }

        if (!(match(TokenType::KW_AS) || matchContextual("AS"))) {
            error("Expected AS in CTE definition");
        }

        if (matchContextual("MATERIALIZED")) {
            cte.materialized = true;
        } else if (matchContextual("NOT")) {
            expectContextual("MATERIALIZED", "Expected MATERIALIZED after NOT");
            cte.not_materialized = true;
        }

        expect(TokenType::LEFT_PAREN, "Expected '(' before CTE query");

        if (check(TokenType::KW_WITH)) {
            cte.query = parseSelectWithClause();
        } else if (match(TokenType::KW_SELECT)) {
            cte.query = parseSelect();
        } else {
            error("Expected SELECT in CTE query");
            synchronize();
        }

        expect(TokenType::RIGHT_PAREN, "Expected ')' after CTE query");
        cte.recursive = with->recursive;

        bool parsed_search_clause = false;
        bool parsed_cycle_clause = false;
        while (true) {
            if (!parsed_search_clause && matchContextual("SEARCH")) {
                parsed_search_clause = true;
                cte.has_search = true;

                if (matchContextual("BREADTH")) {
                    expectContextual("FIRST", "Expected FIRST after SEARCH BREADTH");
                    cte.search_order = CTE::SearchOrder::BREADTH_FIRST;
                } else if (matchContextual("DEPTH")) {
                    expectContextual("FIRST", "Expected FIRST after SEARCH DEPTH");
                    cte.search_order = CTE::SearchOrder::DEPTH_FIRST;
                } else {
                    errorCode("PRS_0504", "SEARCH requires BREADTH FIRST or DEPTH FIRST");
                }

                expectContextual("BY", "Expected BY in SEARCH clause");
                if (!isIdentifier()) {
                    errorCode("PRS_0504", "SEARCH clause requires at least one BY column");
                } else {
                    do {
                        cte.search_by_columns.push_back(expectIdentifier("Expected SEARCH BY column name"));
                    } while (match(TokenType::COMMA));
                }

                if (!(match(TokenType::KW_SET) || matchContextual("SET"))) {
                    errorCode("PRS_0504", "Expected SET in SEARCH clause");
                }
                cte.search_sequence_column = expectIdentifier("Expected SEARCH sequence column after SET");
                continue;
            }

            if (!parsed_cycle_clause && matchContextual("CYCLE")) {
                parsed_cycle_clause = true;
                cte.has_cycle = true;

                if (!isIdentifier()) {
                    errorCode("PRS_0504", "CYCLE clause requires at least one cycle key column");
                } else {
                    do {
                        cte.cycle_columns.push_back(expectIdentifier("Expected CYCLE key column name"));
                    } while (match(TokenType::COMMA));
                }

                if (!(match(TokenType::KW_SET) || matchContextual("SET"))) {
                    errorCode("PRS_0504", "Expected SET in CYCLE clause");
                }
                cte.cycle_mark_column = expectIdentifier("Expected CYCLE mark column after SET");

                if (matchContextual("TO")) {
                    cte.has_cycle_mark_value = true;
                    cte.cycle_mark_value = parseExpression();
                }
                if (match(TokenType::KW_DEFAULT) || matchContextual("DEFAULT")) {
                    cte.has_cycle_default_value = true;
                    cte.cycle_mark_default = parseExpression();
                }

                if (!(match(TokenType::KW_USING) || matchContextual("USING"))) {
                    errorCode("PRS_0504", "Expected USING in CYCLE clause");
                }
                cte.cycle_path_column = expectIdentifier("Expected CYCLE path column after USING");
                continue;
            }

            break;
        }

        with->ctes.push_back(std::move(cte));
    } while (match(TokenType::COMMA));

    return with;
}

SelectStmt* Parser::parseSelectWithClause() {
    WithClause* with = nullptr;
    if (check(TokenType::KW_WITH)) {
        with = parseWithClause();
    }

    if (!match(TokenType::KW_SELECT)) {
        error("Expected SELECT");
        return nullptr;
    }

    auto* stmt = parseSelect();
    stmt->with = with;
    return stmt;
}

// =============================================================================
// SELECT Statement
// =============================================================================

SelectStmt* Parser::parseSelect() {
    SourceLocation start = currentLocation();
    ParseModeGuard guard(state_, ParseMode::DML_SELECT);

    auto* stmt = arena_.create<SelectStmt>();

    // DISTINCT or ALL
    if (matchContextual("DISTINCT")) {
        stmt->distinct = true;
        if (match(TokenType::KW_ON) || matchContextual("ON")) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after DISTINCT ON");
            do {
                stmt->distinct_on.push_back(parseExpression());
            } while (match(TokenType::COMMA));
            expect(TokenType::RIGHT_PAREN, "Expected ')' after DISTINCT ON expression list");
            if (stmt->distinct_on.empty()) {
                errorCode("PRS_0504", "DISTINCT ON requires at least one expression");
            }
        }
    } else if (matchContextual("ALL")) {
        stmt->all = true;
    }

    if (checkContextual("TOP")) {
        Token lookahead = state_.lexer().peekToken();
        bool looks_like_top_clause =
            lookahead.type == TokenType::LEFT_PAREN ||
            lookahead.type == TokenType::INTEGER_LITERAL ||
            lookahead.type == TokenType::FLOAT_LITERAL ||
            lookahead.type == TokenType::PARAMETER ||
            lookahead.type == TokenType::PLUS ||
            lookahead.type == TokenType::MINUS;
        if (looks_like_top_clause) {
            matchContextual("TOP");
            errorCode("PRS_0505",
                      "TOP(...) [PERCENT] [WITH TIES] is not supported in v3; use LIMIT/OFFSET or FETCH FIRST");

            if (match(TokenType::LEFT_PAREN)) {
                int depth = 1;
                while (!isAtEnd() && depth > 0) {
                    if (match(TokenType::LEFT_PAREN)) {
                        ++depth;
                    } else if (match(TokenType::RIGHT_PAREN)) {
                        --depth;
                    } else {
                        advance();
                    }
                }
            } else if (!isAtEnd()) {
                advance();
            }

            matchContextual("PERCENT");
            if (match(TokenType::KW_WITH) || matchContextual("WITH")) {
                matchContextual("TIES");
            }
        }
    }

    auto parseFirebirdRowCountExpr = [&]() -> Expression* {
        if (match(TokenType::LEFT_PAREN)) {
            Expression* expr = parseExpression();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after row-count expression");
            return expr;
        }
        if (check(TokenType::INTEGER_LITERAL)) {
            Token value = current();
            advance();
            auto* lit = arena_.create<LiteralExpr>();
            lit->literal_type = LiteralType::INTEGER;
            lit->int_value = value.value.int_value;
            return lit;
        }
        return parseExpression();
    };
    auto makeIntLiteral = [&](int64_t value) -> LiteralExpr* {
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::INTEGER;
        lit->int_value = value;
        return lit;
    };

    // Firebird SELECT FIRST/SKIP clause (pre-select-list).
    if (matchContextual("FIRST")) {
        stmt->limit = parseFirebirdRowCountExpr();
    }
    if (matchContextual("SKIP")) {
        stmt->offset = parseFirebirdRowCountExpr();
    }

    // Parse select list
    parseSelectList(stmt);

    // FROM clause (optional for SELECT without tables)
    if (match(TokenType::KW_FROM)) {
        parseFromClause(stmt);
    }

    // WHERE clause
    if (match(TokenType::KW_WHERE)) {
        parseWhereClause(stmt);
    }

    auto reject_connect_by_surface = [&]() {
        errorCode("PRS_0505",
                  "START WITH ... CONNECT BY is not supported in v3; use WITH RECURSIVE");
        while (!isAtEnd() &&
               !check(TokenType::KW_GROUP) &&
               !check(TokenType::KW_HAVING) &&
               !check(TokenType::KW_ORDER) &&
               !check(TokenType::KW_LIMIT) &&
               !check(TokenType::KW_OFFSET) &&
               !check(TokenType::KW_UNION) &&
               !check(TokenType::KW_INTERSECT) &&
               !check(TokenType::KW_EXCEPT) &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE) &&
               !checkContextual("FETCH") &&
               !checkContextual("FOR")) {
            advance();
        }
    };

    if (check(TokenType::KW_START)) {
        Token lookahead = state_.lexer().peekToken();
        if (lookahead.type == TokenType::KW_WITH ||
            (lookahead.type == TokenType::IDENTIFIER &&
             caseInsensitiveEquals(state_.lexer().getTokenText(lookahead.span), "WITH"))) {
            match(TokenType::KW_START);
            match(TokenType::KW_WITH) || matchContextual("WITH");
            reject_connect_by_surface();
        }
    }

    if (checkContextual("CONNECT")) {
        Token lookahead = state_.lexer().peekToken();
        if (lookahead.type == TokenType::IDENTIFIER &&
            caseInsensitiveEquals(state_.lexer().getTokenText(lookahead.span), "BY")) {
            matchContextual("CONNECT");
            matchContextual("BY");
            reject_connect_by_surface();
        }
    }

    // GROUP BY clause
    if (match(TokenType::KW_GROUP)) {
        expectContextual("BY", "Expected BY after GROUP");
        parseGroupByClause(stmt);
    }

    // HAVING clause
    if (match(TokenType::KW_HAVING)) {
        parseHavingClause(stmt);
    }

    // ORDER BY clause (ORDER is a Gatekeeper keyword)
    if (match(TokenType::KW_ORDER)) {
        expectContextual("BY", "Expected BY after ORDER");
        parseOrderByClause(stmt);
        // Resolve ORDER BY numeric positions (e.g., ORDER BY 1)
        for (auto* item : stmt->order_by) {
            if (!item || !item->expr) {
                continue;
            }
            if (item->expr->kind() != ASTKind::LiteralExpr) {
                continue;
            }
            auto* lit = static_cast<LiteralExpr*>(item->expr);
            if (lit->literal_type != LiteralType::INTEGER) {
                continue;
            }
            if (lit->int_value <= 0 ||
                static_cast<size_t>(lit->int_value) > stmt->items.size()) {
                error("ORDER BY position out of range");
                continue;
            }
            SelectItem* sel = stmt->items[static_cast<size_t>(lit->int_value - 1)];
            if (!sel || sel->item_type != SelectItem::Type::EXPRESSION || !sel->expr) {
                error("ORDER BY position must reference a select expression");
                continue;
            }
            item->expr = sel->expr;
        }
    }

    // LIMIT/OFFSET clause (LIMIT is a Gatekeeper keyword)
    if (match(TokenType::KW_LIMIT)) {
        parseLimitClause(stmt);
    }

    // OFFSET without LIMIT (PostgreSQL style)
    if (matchContextual("OFFSET")) {
        stmt->offset = parseExpression();
        if (matchContextual("ROWS") || matchContextual("ROW")) {
            // Optional ROWS/ROW keyword
        }
    }

    auto check_ident_word = [&](const char* keyword) -> bool {
        if (!isIdentifier()) {
            return false;
        }
        StringPool::StringId id = state_.currentStringId();
        if (id == StringPool::INVALID_ID) {
            return false;
        }
        return toUpperAscii(std::string(stringPool().get(id))) == keyword;
    };
    auto match_ident_word = [&](const char* keyword) -> bool {
        if (!check_ident_word(keyword)) {
            return false;
        }
        advance();
        return true;
    };

    // FETCH clause (SQL:2008 standard with PostgreSQL WITH TIES extension)
    if (match_ident_word("FETCH")) {
        if (stmt->limit) {
            errorCode("PRS_0504", "FETCH clause conflicts with LIMIT");
        }

        bool saw_mode = false;
        if (match_ident_word("FIRST")) {
            stmt->fetch_mode = FetchMode::FIRST;
            saw_mode = true;
        } else if (match_ident_word("NEXT")) {
            stmt->fetch_mode = FetchMode::NEXT;
            saw_mode = true;
        }

        if (!saw_mode) {
            errorCode("PRS_0505", "Expected FIRST or NEXT after FETCH");
        }

        if (!check_ident_word("ROW") && !check_ident_word("ROWS")) {
            if (check(TokenType::INTEGER_LITERAL)) {
                Token count_token = current();
                advance();
                auto* count = arena_.create<LiteralExpr>();
                count->literal_type = LiteralType::INTEGER;
                count->int_value = count_token.value.int_value;
                stmt->fetch_row_count = count;
            } else {
                stmt->fetch_row_count = parseExpression();
            }
        }

        if (!stmt->fetch_row_count) {
            auto* one = arena_.create<LiteralExpr>();
            one->literal_type = LiteralType::INTEGER;
            one->int_value = 1;
            stmt->fetch_row_count = one;
        }
        stmt->limit = stmt->fetch_row_count;

        if (match_ident_word("ROW") || match_ident_word("ROWS")) {
            if (match_ident_word("ONLY")) {
                stmt->fetch_with_ties = false;
            } else if (match(TokenType::KW_WITH) || match_ident_word("WITH")) {
                if (!match_ident_word("TIES")) {
                    errorCode("PRS_0505", "Expected TIES after WITH in FETCH clause");
                }
                if (stmt->order_by.empty()) {
                    errorCode("PRS_0504", "FETCH ... WITH TIES requires ORDER BY");
                }
                stmt->fetch_with_ties = true;
            } else {
                errorCode("PRS_0505", "Expected ONLY or WITH TIES after FETCH ... ROWS");
            }
        } else {
            errorCode("PRS_0505", "Expected ROW or ROWS in FETCH clause");
        }
    }

    // Firebird ROWS m [TO n] clause
    if (matchContextual("ROWS")) {
        Expression* start_expr = parseFirebirdRowCountExpr();
        if (matchContextual("TO")) {
            Expression* end_expr = parseFirebirdRowCountExpr();

            auto* offset_expr = arena_.create<BinaryExpr>();
            offset_expr->op = BinaryOp::SUB;
            offset_expr->left = start_expr;
            offset_expr->right = makeIntLiteral(1);
            stmt->offset = offset_expr;

            auto* delta_expr = arena_.create<BinaryExpr>();
            delta_expr->op = BinaryOp::SUB;
            delta_expr->left = end_expr;
            delta_expr->right = start_expr;

            auto* limit_expr = arena_.create<BinaryExpr>();
            limit_expr->op = BinaryOp::ADD;
            limit_expr->left = delta_expr;
            limit_expr->right = makeIntLiteral(1);
            stmt->limit = limit_expr;
        } else {
            stmt->limit = start_expr;
        }
    }

    // Firebird PLAN clause (captured as expression tree for deterministic payload).
    if (matchContextual("PLAN")) {
        stmt->firebird_plan = parseExpression();
    }

    // Firebird OPTIMIZE FOR <n> ROWS
    if (matchContextual("OPTIMIZE")) {
        expectContextual("FOR", "Expected FOR after OPTIMIZE");
        stmt->optimize_for_rows = parseFirebirdRowCountExpr();
        matchContextual("ROWS");
        matchContextual("ROW");
    }

    // Set operations (UNION, INTERSECT, EXCEPT)
    if (!stmt->distinct_on.empty() && stmt->order_by.empty()) {
        errorCode("PRS_0504", "DISTINCT ON requires ORDER BY for deterministic tie-breaking");
    }

    parseSetOperation(stmt);

    // FOR UPDATE/SHARE (FOR is contextual)
    if (matchContextual("FOR")) {
        if (match(TokenType::KW_UPDATE)) {
            stmt->lock_strength = SelectLockStrength::UPDATE;
            stmt->for_update = true;
        } else if (matchContextual("SHARE")) {
            stmt->lock_strength = SelectLockStrength::SHARE;
            stmt->for_share = true;
        } else if (matchContextual("NO")) {
            // FOR NO KEY UPDATE
            expectContextual("KEY", "Expected KEY after NO");
            if (!match(TokenType::KW_UPDATE)) {
                errorCode("PRS_0505", "Expected UPDATE after FOR NO KEY");
            }
            stmt->lock_strength = SelectLockStrength::NO_KEY_UPDATE;
            stmt->for_update = true;
        } else if (matchContextual("KEY")) {
            // FOR KEY SHARE
            expectContextual("SHARE", "Expected SHARE after KEY");
            stmt->lock_strength = SelectLockStrength::KEY_SHARE;
            stmt->for_share = true;
        } else {
            errorCode("PRS_0505", "Expected UPDATE, SHARE, NO KEY UPDATE, or KEY SHARE after FOR");
        }

        // OF table_name - skip tables, AST doesn't support this
        if (matchContextual("OF")) {
            do {
                parseSchemaPath(state_);  // Parse but ignore
            } while (match(TokenType::COMMA));
        }

        // Firebird/PostgreSQL lock modifiers.
        while (true) {
            if (match(TokenType::KW_WITH) || matchContextual("WITH")) {
                expectContextual("LOCK", "Expected LOCK after WITH");
                stmt->with_lock = true;
                continue;
            }
            if (matchContextual("NOWAIT")) {
                stmt->nowait = true;
                continue;
            }
            if (matchContextual("SKIP")) {
                expectContextual("LOCKED", "Expected LOCKED after SKIP");
                stmt->skip_locked = true;
                continue;
            }
            break;
        }

        if (stmt->fetch_with_ties && stmt->skip_locked) {
            errorCode("PRS_0504", "FETCH ... WITH TIES cannot be combined with SKIP LOCKED");
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

void Parser::parseSelectList(SelectStmt* stmt) {
    do {
        SelectItem* item = parseSelectItem();
        if (item) {
            stmt->items.push_back(item);
        }
    } while (match(TokenType::COMMA));
}

SelectItem* Parser::parseSelectItem() {
    auto* item = arena_.create<SelectItem>();
    SourceLocation start = currentLocation();
    auto isContextFunctionKeyword = [&]() {
        return checkContextual("CURRENT_USER") ||
               checkContextual("SESSION_USER") ||
               checkContextual("CURRENT_ROLE") ||
               checkContextual("CURRENT_CONNECTION") ||
               checkContextual("CURRENT_SESSION") ||
               checkContextual("CURRENT_TRANSACTION") ||
               checkContextual("NOW") ||
               checkContextual("CURRENT_DATE") ||
               checkContextual("CURRENT_TIME") ||
               checkContextual("CURRENT_TIMESTAMP");
    };

    // Check for * (select all)
    if (match(TokenType::STAR)) {
        item->item_type = SelectItem::Type::STAR;
        item->span = makeSpan(start);
        return item;
    }

    // Check for table.* (qualified star)
    if (isIdentifier()) {
        if (checkContextual("ARRAY")) {
            Token next = state_.lexer().peekToken();
            if (next.type == TokenType::LEFT_BRACKET) {
                item->item_type = SelectItem::Type::EXPRESSION;
                item->expr = parseExpression();
                item->span = makeSpan(start);
                return item;
            }
        }
        if (isContextFunctionKeyword()) {
            item->item_type = SelectItem::Type::EXPRESSION;
            item->expr = parseExpression();
            item->span = makeSpan(start);
            return item;
        }
        SchemaPath path = parseSchemaPath(state_);
        bool saw_trailing_dot = (previous().type == TokenType::DOT);

        if (saw_trailing_dot && match(TokenType::STAR)) {
            item->item_type = SelectItem::Type::TABLE_STAR;
            item->table_path = std::move(path);
            item->span = makeSpan(start);
            return item;
        }

        item->item_type = SelectItem::Type::EXPRESSION;
        Expression* left = nullptr;
        if (check(TokenType::LEFT_PAREN)) {
            left = parseFunctionCall(std::move(path));
        } else {
            auto* colRef = arena_.create<ColumnRefExpr>();
            if (path.components.size() == 1) {
                colRef->column.column_name = path.components[0];
            } else {
                colRef->column.column_name = path.objectName();
                colRef->column.has_table_qualifier = true;
                colRef->column.table_path.type = path.type;
                colRef->column.table_path.components = path.schemaComponents();
            }
            left = colRef;
        }
        item->expr = parseExpressionWithLeft(left);
    } else {
        // Parse expression
        item->item_type = SelectItem::Type::EXPRESSION;
        item->expr = parseExpression();
    }

    // Optional alias
    if (match(TokenType::KW_AS)) {
        item->alias = expectIdentifier("Expected alias after AS");
        item->has_alias = true;
    } else if (isIdentifier() &&
               !check(TokenType::KW_FROM) &&
               !check(TokenType::COMMA) &&
               !checkContextual("MINUS")) {
        // Alias without AS keyword
        item->alias = currentIdentifier();
        item->has_alias = true;
    }

    item->span = makeSpan(start);
    return item;
}

void Parser::parseFromClause(SelectStmt* stmt) {
    ParseModeGuard guard(state_, ParseMode::TABLE_REF);

    // Parse first table reference
    stmt->from = parseTableRef();

    auto is_natural_join_lead = [&]() -> bool {
        if (!checkContextual("NATURAL")) {
            return false;
        }
        Token lookahead = state_.lexer().peekToken();
        if (lookahead.type == TokenType::KW_JOIN) {
            return true;
        }
        if (lookahead.type != TokenType::IDENTIFIER) {
            return false;
        }
        std::string next = toUpperAscii(std::string(state_.lexer().getTokenText(lookahead.span)));
        return next == "LEFT" || next == "RIGHT" || next == "FULL" || next == "INNER";
    };

    // Parse joins
    while (true) {
        // Check for join keywords
        if (check(TokenType::KW_JOIN) ||
            check(TokenType::KW_ON) ||
            checkContextual("LEFT") ||
            checkContextual("RIGHT") ||
            checkContextual("INNER") ||
            checkContextual("OUTER") ||
            checkContextual("FULL") ||
            checkContextual("CROSS") ||
            is_natural_join_lead()) {

            JoinNode* join = parseJoin(stmt->from);
            if (join) {
                stmt->joins.push_back(join);
            }
        } else if (match(TokenType::COMMA)) {
            // Cross join (implicit)
            auto* join = arena_.create<JoinNode>();
            join->join_type = JoinType::CROSS;
            join->right = parseTableRef();
            stmt->joins.push_back(join);
        } else {
            break;
        }
    }
}

TableRefNode* Parser::parseTableRef() {
    auto* node = arena_.create<TableRefNode>();
    SourceLocation start = currentLocation();

    // Check for subquery / lateral / table/function source
    if (check(TokenType::LEFT_PAREN)) {
        advance();  // consume (

        // Check if it's a SELECT/CTE (subquery) or just a grouped table reference
        if (check(TokenType::KW_SELECT) || check(TokenType::KW_WITH)) {
            node->ref_type = TableRefNode::Type::SUBQUERY;
            node->subquery = parseSelectWithClause();
        } else {
            // Could be a nested table reference - parse as expression for now
            // and let it be resolved later
            node->ref_type = TableRefNode::Type::TABLE;
            node->table_path = parseSchemaPath(state_);
        }

        expect(TokenType::RIGHT_PAREN, "Expected ')' after subquery");
    } else if (match(TokenType::KW_LATERAL) || matchContextual("LATERAL")) {
        node->lateral = true;

        if (check(TokenType::LEFT_PAREN)) {
            // LATERAL (SELECT ...)
            advance();  // consume (
            if (check(TokenType::KW_SELECT) || check(TokenType::KW_WITH)) {
                node->ref_type = TableRefNode::Type::SUBQUERY;
                node->subquery = parseSelectWithClause();
            } else {
                error("Expected SELECT after LATERAL");
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after LATERAL subquery");
        } else {
            // LATERAL function/table source
            node->table_path = parseSchemaPath(state_);
            if (check(TokenType::LEFT_PAREN)) {
                node->ref_type = TableRefNode::Type::FUNCTION;
                auto* funcExpr = dynamic_cast<FunctionCallExpr*>(parseFunctionCall(std::move(node->table_path)));
                node->function = funcExpr;
                if (match(TokenType::KW_WITH) || matchContextual("WITH")) {
                    expectContextual("ORDINALITY", "Expected ORDINALITY after WITH");
                    node->with_ordinality = true;
                }
            } else {
                node->ref_type = TableRefNode::Type::TABLE;
            }
        }
    } else {
        // Table name or function
        node->table_path = parseSchemaPath(state_);

        // Check for function call syntax
        if (check(TokenType::LEFT_PAREN)) {
            node->ref_type = TableRefNode::Type::FUNCTION;
            auto* funcExpr = dynamic_cast<FunctionCallExpr*>(parseFunctionCall(std::move(node->table_path)));
            node->function = funcExpr;
            if (match(TokenType::KW_WITH) || matchContextual("WITH")) {
                expectContextual("ORDINALITY", "Expected ORDINALITY after WITH");
                node->with_ordinality = true;
            }
        } else {
            node->ref_type = TableRefNode::Type::TABLE;
        }
    }

    auto is_dual_reference = [&](const SchemaPath& path) -> bool {
        return path.type == PathType::UNQUALIFIED &&
               path.components.size() == 1 &&
               caseInsensitiveEquals(stringPool().get(path.components.front()), "DUAL");
    };
    if (node->ref_type == TableRefNode::Type::TABLE && is_dual_reference(node->table_path)) {
        errorCode("PRS_0505",
                  "FROM DUAL is not supported in v3; use SELECT without FROM or FROM (VALUES (1))");
    }

    // PostgreSQL TABLESAMPLE support.
    if (matchContextual("TABLESAMPLE")) {
        if (node->ref_type != TableRefNode::Type::TABLE) {
            errorCode("PRS_0504", "TABLESAMPLE is only valid for base table references");
        }

        if (matchContextual("BERNOULLI")) {
            node->sample_method = TableSampleMethod::BERNOULLI;
        } else if (matchContextual("SYSTEM")) {
            node->sample_method = TableSampleMethod::SYSTEM;
        } else {
            errorCode("PRS_0504", "TABLESAMPLE method must be BERNOULLI or SYSTEM");
        }

        expect(TokenType::LEFT_PAREN, "Expected '(' after TABLESAMPLE method");
        if (check(TokenType::RIGHT_PAREN)) {
            errorCode("PRS_0504", "TABLESAMPLE requires sampling percentage expression");
        } else {
            node->sample_percent = parseExpression();
        }
        if (match(TokenType::COMMA)) {
            errorCode("PRS_0504", "TABLESAMPLE accepts exactly one sampling argument");
            while (!check(TokenType::RIGHT_PAREN) &&
                   !check(TokenType::END_OF_FILE)) {
                advance();
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after TABLESAMPLE argument");

        if (matchContextual("REPEATABLE")) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after REPEATABLE");
            node->sample_repeatable_seed = parseExpression();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after REPEATABLE seed");
        }
    }

    // Optional alias
    if (match(TokenType::KW_AS)) {
        node->alias = expectIdentifier("Expected alias after AS");
        node->has_alias = true;
    } else if (isIdentifier() &&
               !check(TokenType::KW_WHERE) &&
               !check(TokenType::KW_JOIN) &&
               !check(TokenType::KW_ON) &&
               !check(TokenType::KW_GROUP) &&
               !check(TokenType::KW_ORDER) &&
               !check(TokenType::KW_HAVING) &&
               !check(TokenType::KW_LIMIT) &&
               !check(TokenType::KW_UNION) &&
               !check(TokenType::KW_INTERSECT) &&
               !check(TokenType::KW_EXCEPT) &&
               !checkContextual("MINUS") &&
               !check(TokenType::COMMA) &&
               !checkContextual("LEFT") &&
               !checkContextual("RIGHT") &&
               !checkContextual("INNER") &&
               !checkContextual("OUTER") &&
               !checkContextual("FULL") &&
               !checkContextual("CROSS") &&
               !checkContextual("NATURAL") &&
               !checkContextual("PLAN") &&
               !checkContextual("OPTIMIZE") &&
               !checkContextual("ROWS") &&
               !checkContextual("PIVOT") &&
               !checkContextual("UNPIVOT") &&
               !checkContextual("APPLY") &&
               !checkContextual("FOR") &&
               !checkContextual("OFFSET") &&
               !checkContextual("FETCH") &&
               !checkContextual("RETURNING")) {
        // Alias without AS keyword
        node->alias = currentIdentifier();
        node->has_alias = true;
    }

    // Optional column aliases: table AS alias(col1, col2)
    if (node->has_alias && check(TokenType::LEFT_PAREN)) {
        advance();
        do {
            node->column_aliases.push_back(expectIdentifier("Expected column alias"));
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after column aliases");
    }

    if (matchContextual("PIVOT") || matchContextual("UNPIVOT")) {
        errorCode("PRS_0505",
                  "PIVOT/UNPIVOT is not supported in v3; use canonical CASE/aggregate or UNION ALL rewrites");
    }
    if (matchContextual("APPLY")) {
        errorCode("PRS_0505",
                  "CROSS APPLY/OUTER APPLY is not supported in v3; use JOIN LATERAL or LEFT JOIN LATERAL");
    }

    node->span = makeSpan(start);
    return node;
}

JoinType Parser::parseJoinType() {
    bool natural = false;
    if (matchContextual("NATURAL")) {
        natural = true;
    }

    JoinType type = JoinType::INNER;

    if (matchContextual("LEFT")) {
        matchContextual("OUTER");  // optional
        type = natural ? JoinType::NATURAL_LEFT : JoinType::LEFT;
    } else if (matchContextual("RIGHT")) {
        matchContextual("OUTER");  // optional
        type = natural ? JoinType::NATURAL_RIGHT : JoinType::RIGHT;
    } else if (matchContextual("FULL")) {
        matchContextual("OUTER");  // optional
        type = natural ? JoinType::NATURAL_FULL : JoinType::FULL;
    } else if (matchContextual("CROSS")) {
        type = JoinType::CROSS;
    } else if (matchContextual("INNER")) {
        type = natural ? JoinType::NATURAL : JoinType::INNER;
    } else if (natural) {
        type = JoinType::NATURAL;
    }

    return type;
}

JoinNode* Parser::parseJoin(TableRefNode* left) {
    auto* join = arena_.create<JoinNode>();
    SourceLocation start = currentLocation();

    join->left = left;

    if (checkContextual("OUTER")) {
        Token lookahead = state_.lexer().peekToken();
        if (lookahead.type == TokenType::IDENTIFIER &&
            caseInsensitiveEquals(state_.lexer().getTokenText(lookahead.span), "APPLY")) {
            matchContextual("OUTER");
            matchContextual("APPLY");
            errorCode("PRS_0505",
                      "OUTER APPLY is not supported in v3; use LEFT JOIN LATERAL ... ON TRUE");
            join->join_type = JoinType::LEFT;
            join->right = parseTableRef();
            join->span = makeSpan(start);
            return join;
        }
    }

    join->join_type = parseJoinType();

    if (matchContextual("APPLY")) {
        errorCode("PRS_0505",
                  "CROSS APPLY/OUTER APPLY is not supported in v3; use JOIN LATERAL or LEFT JOIN LATERAL");
        join->right = parseTableRef();
        join->span = makeSpan(start);
        return join;
    }

    // JOIN keyword
    expect(TokenType::KW_JOIN, "Expected JOIN");

    // Right table
    join->right = parseTableRef();

    // ON or USING clause (not for CROSS or NATURAL joins)
    if (join->join_type != JoinType::CROSS &&
        join->join_type != JoinType::NATURAL &&
        join->join_type != JoinType::NATURAL_LEFT &&
        join->join_type != JoinType::NATURAL_RIGHT &&
        join->join_type != JoinType::NATURAL_FULL) {

        if (match(TokenType::KW_ON)) {
            join->on_condition = parseExpression();
        } else if (match(TokenType::KW_USING)) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after USING");
            do {
                join->using_columns.push_back(expectIdentifier("Expected column name"));
            } while (match(TokenType::COMMA));
            join->has_using = true;
            expect(TokenType::RIGHT_PAREN, "Expected ')' after USING columns");
        }
    }

    join->span = makeSpan(start);
    return join;
}

void Parser::parseWhereClause(SelectStmt* stmt) {
    ParseModeGuard guard(state_, ParseMode::EXPRESSION);
    stmt->where = parseExpression();
}

void Parser::parseGroupByClause(SelectStmt* stmt) {
    if (matchContextual("ROLLUP")) {
        stmt->grouping_type = GroupingType::ROLLUP;
        expect(TokenType::LEFT_PAREN, "Expected '(' after ROLLUP");
        do {
            stmt->group_by.push_back(parseExpression());
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after ROLLUP list");
        return;
    }

    if (matchContextual("CUBE")) {
        stmt->grouping_type = GroupingType::CUBE;
        expect(TokenType::LEFT_PAREN, "Expected '(' after CUBE");
        do {
            stmt->group_by.push_back(parseExpression());
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after CUBE list");
        return;
    }

    if (matchContextual("GROUPING")) {
        if (!matchContextual("SETS")) {
            error("Expected SETS after GROUPING");
        }
        stmt->grouping_type = GroupingType::GROUPING_SETS;
        expect(TokenType::LEFT_PAREN, "Expected '(' after GROUPING SETS");

        do {
            std::vector<Expression*> grouping_set;
            if (match(TokenType::LEFT_PAREN)) {
                if (!check(TokenType::RIGHT_PAREN)) {
                    do {
                        grouping_set.push_back(parseExpression());
                    } while (match(TokenType::COMMA));
                }
                expect(TokenType::RIGHT_PAREN, "Expected ')' after grouping set");
            } else {
                grouping_set.push_back(parseExpression());
            }
            stmt->grouping_sets.push_back(std::move(grouping_set));
        } while (match(TokenType::COMMA));

        expect(TokenType::RIGHT_PAREN, "Expected ')' after GROUPING SETS list");
        return;
    }

    do {
        stmt->group_by.push_back(parseExpression());
    } while (match(TokenType::COMMA));
}

void Parser::parseHavingClause(SelectStmt* stmt) {
    ParseModeGuard guard(state_, ParseMode::EXPRESSION);
    stmt->having = parseExpression();
}

void Parser::parseOrderByClause(SelectStmt* stmt) {
    do {
        OrderByItem* item = parseOrderByItem();
        if (item) {
            stmt->order_by.push_back(item);
        }
    } while (match(TokenType::COMMA));
}

OrderByItem* Parser::parseOrderByItem() {
    auto* item = arena_.create<OrderByItem>();
    SourceLocation start = currentLocation();

    item->expr = parseExpression();

    // ASC or DESC
    if (matchContextual("ASC")) {
        item->ascending = true;
    } else if (matchContextual("DESC")) {
        item->ascending = false;
    }

    // NULLS FIRST or NULLS LAST
    if (matchContextual("NULLS")) {
        if (matchContextual("FIRST")) {
            item->nulls_first = true;
        } else if (matchContextual("LAST")) {
            item->nulls_last = true;
        }
    }

    item->span = makeSpan(start);
    return item;
}

WindowSpec* Parser::parseWindowSpec() {
    auto* spec = arena_.create<WindowSpec>();

    expect(TokenType::LEFT_PAREN, "Expected '(' after OVER");

    if (!check(TokenType::RIGHT_PAREN)) {
        if (matchContextual("PARTITION")) {
            expectContextual("BY", "Expected BY after PARTITION");
            do {
                spec->partition_by.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }

        if (match(TokenType::KW_ORDER) || matchContextual("ORDER")) {
            expectContextual("BY", "Expected BY after ORDER");
            do {
                OrderByItem* item = parseOrderByItem();
                if (item) {
                    spec->order_by.push_back(item);
                }
            } while (match(TokenType::COMMA));
        }

        if (checkContextual("ROWS") || checkContextual("RANGE") || checkContextual("GROUPS")) {
            parseWindowFrame(spec);
        }
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after window specification");
    return spec;
}

void Parser::parseWindowFrame(WindowSpec* spec) {
    if (!spec) {
        error("Window specification required for frame clause");
        return;
    }

    if (matchContextual("ROWS")) {
        spec->frame_type = FrameType::ROWS;
    } else if (matchContextual("RANGE")) {
        spec->frame_type = FrameType::RANGE;
    } else if (matchContextual("GROUPS")) {
        spec->frame_type = FrameType::GROUPS;
    } else {
        return;
    }

    spec->has_frame = true;

    if (match(TokenType::KW_BETWEEN) || matchContextual("BETWEEN")) {
        spec->frame_start = parseWindowFrameBound(&spec->frame_start_value);
        if (!(match(TokenType::KW_AND) || matchContextual("AND"))) {
            error("Expected AND in window frame");
        }
        spec->frame_end = parseWindowFrameBound(&spec->frame_end_value);
    } else {
        spec->frame_start = parseWindowFrameBound(&spec->frame_start_value);
        spec->frame_end = FrameBoundType::CURRENT_ROW;
        spec->frame_end_value = nullptr;
    }

    if (matchContextual("EXCLUDE")) {
        spec->has_frame_exclusion = true;
        if (matchContextual("NO")) {
            expectContextual("OTHERS", "Expected OTHERS after EXCLUDE NO");
            spec->frame_exclusion = FrameExclusion::NO_OTHERS;
        } else if (matchContextual("CURRENT")) {
            expectContextual("ROW", "Expected ROW after EXCLUDE CURRENT");
            spec->frame_exclusion = FrameExclusion::CURRENT_ROW;
        } else if (matchContextual("GROUP")) {
            spec->frame_exclusion = FrameExclusion::GROUP;
        } else if (matchContextual("TIES")) {
            spec->frame_exclusion = FrameExclusion::TIES;
        } else {
            errorCode("PRS_0504", "Expected NO OTHERS, CURRENT ROW, GROUP, or TIES after EXCLUDE");
        }
    }
}

FrameBoundType Parser::parseWindowFrameBound(Expression** value_out) {
    if (value_out) {
        *value_out = nullptr;
    }

    if (matchContextual("UNBOUNDED")) {
        if (matchContextual("PRECEDING")) {
            return FrameBoundType::UNBOUNDED_PRECEDING;
        }
        if (matchContextual("FOLLOWING")) {
            return FrameBoundType::UNBOUNDED_FOLLOWING;
        }
        error("Expected PRECEDING or FOLLOWING after UNBOUNDED");
        return FrameBoundType::UNBOUNDED_PRECEDING;
    }

    if (matchContextual("CURRENT")) {
        expectContextual("ROW", "Expected ROW after CURRENT");
        return FrameBoundType::CURRENT_ROW;
    }

    Expression* value = parseExpression();
    if (matchContextual("PRECEDING")) {
        if (value_out) {
            *value_out = value;
        }
        return FrameBoundType::VALUE_PRECEDING;
    }
    if (matchContextual("FOLLOWING")) {
        if (value_out) {
            *value_out = value;
        }
        return FrameBoundType::VALUE_FOLLOWING;
    }

    error("Expected PRECEDING or FOLLOWING after window frame offset");
    return FrameBoundType::CURRENT_ROW;
}

void Parser::parseLimitClause(SelectStmt* stmt) {
    // LIMIT ALL or LIMIT expression
    if (matchContextual("ALL")) {
        // LIMIT ALL means no limit
    } else {
        stmt->limit = parseExpression();
    }

    // Optional OFFSET (OFFSET is a Gatekeeper keyword)
    if (match(TokenType::KW_OFFSET)) {
        stmt->offset = parseExpression();
    }
}

void Parser::parseSetOperation(SelectStmt* stmt) {
    SetOpType op = SetOpType::NONE;

    if (match(TokenType::KW_UNION)) {
        op = SetOpType::UNION;
    } else if (match(TokenType::KW_INTERSECT)) {
        op = SetOpType::INTERSECT;
    } else if (match(TokenType::KW_EXCEPT)) {
        op = SetOpType::EXCEPT;
    } else if (match(TokenType::MINUS) || matchContextual("MINUS")) {
        errorCode("PRS_0505", "MINUS is not supported in v3; use EXCEPT");
        op = SetOpType::EXCEPT;
    } else {
        return;
    }

    stmt->set_op = op;

    // ALL modifier
    if (matchContextual("ALL")) {
        stmt->set_op_all = true;
    }

    // Parse right side SELECT
    bool parenthesized = false;
    if (match(TokenType::KW_SELECT)) {
        stmt->set_op_right = parseSelect();
    } else if (check(TokenType::LEFT_PAREN)) {
        // Parenthesized SELECT
        parenthesized = true;
        advance();
        if (match(TokenType::KW_SELECT)) {
            stmt->set_op_right = parseSelect();
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after SELECT");
    } else {
        error("Expected SELECT after set operation");
    }

    if (stmt->set_op_right && stmt->set_op_right->set_op != SetOpType::NONE && !parenthesized) {
        error("Chained set operations require parentheses to disambiguate");
    }
}

// =============================================================================
// INSERT Statement
// =============================================================================

InsertStmt* Parser::parseInsert() {
    SourceLocation start = currentLocation();
    ParseModeGuard guard(state_, ParseMode::DML_INSERT);

    auto* stmt = arena_.create<InsertStmt>();

    if (matchContextual("IGNORE")) {
        errorCode("PRS_0505",
                  "INSERT IGNORE is not supported in v3; use INSERT ... ON CONFLICT DO NOTHING");
    }

    if (matchContextual("OVERRIDING")) {
        if (matchContextual("SYSTEM")) {
            stmt->overriding = InsertStmt::OverridingMode::SYSTEM;
        } else if (matchContextual("USER")) {
            stmt->overriding = InsertStmt::OverridingMode::USER;
        } else {
            errorCode("PRS_0505", "Expected SYSTEM or USER after OVERRIDING");
        }
        expectContextual("VALUE", "Expected VALUE after OVERRIDING mode");
    }

    // INTO (optional in some databases)
    match(TokenType::KW_INTO);

    // Table name
    stmt->table_path = parseSchemaPath(state_);

    // Optional alias
    if (match(TokenType::KW_AS)) {
        stmt->alias = expectIdentifier("Expected alias after AS");
        stmt->has_alias = true;
    } else if (isIdentifier() && !check(TokenType::LEFT_PAREN) &&
               !check(TokenType::KW_VALUES) && !check(TokenType::KW_SELECT) &&
               !check(TokenType::KW_DEFAULT)) {
        stmt->alias = currentIdentifier();
        stmt->has_alias = true;
    }

    // Column list (optional)
    if (check(TokenType::LEFT_PAREN) && !check(TokenType::KW_SELECT)) {
        parseInsertColumns(stmt);
    }

    // VALUES, SELECT, or DEFAULT VALUES
    if (match(TokenType::KW_VALUES)) {
        stmt->source = InsertStmt::Source::VALUES;
        parseValuesClause(stmt);
    } else if (match(TokenType::KW_SELECT)) {
        stmt->source = InsertStmt::Source::SELECT;
        stmt->select_source = parseSelect();
    } else if (match(TokenType::KW_DEFAULT)) {
        expect(TokenType::KW_VALUES, "Expected VALUES after DEFAULT");
        stmt->source = InsertStmt::Source::DEFAULT;
    }

    // ON CONFLICT clause
    if (match(TokenType::KW_ON)) {
        if (matchContextual("CONFLICT")) {
            parseOnConflict(stmt);
        } else if (matchContextual("DUPLICATE")) {
            if (!requireFeature(kFeatureDmlMysqlOnDuplicateKey)) {
                return stmt;
            }
            errorCode("PRS_0505",
                      "ON DUPLICATE KEY UPDATE is not supported in v3; use ON CONFLICT DO UPDATE");
            if (matchContextual("KEY")) {
                match(TokenType::KW_UPDATE) || matchContextual("UPDATE");
            }
            while (!isAtEnd() &&
                   !checkContextual("RETURNING") &&
                   !check(TokenType::SEMICOLON) &&
                   !check(TokenType::END_OF_FILE)) {
                advance();
            }
        } else {
            errorCode("PRS_0505", "Only ON CONFLICT is supported after INSERT in v3");
        }
    }

    // Per-operation consistency controls (native canonical WITH and alias USING).
    parseConsistencyClause(stmt->consistency_level, stmt->serial_consistency_level);

    // Conditional write controls (native canonical WHEN and alias IF).
    parseConditionalWriteClause(true,
                                stmt->conditional_if_exists,
                                stmt->conditional_if_not_exists,
                                stmt->conditional_if);

    if (matchContextual("OUTPUT")) {
        errorCode("PRS_0505", "OUTPUT clause is not supported in v3; use RETURNING");
        while (!isAtEnd() &&
               !checkContextual("RETURNING") &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            advance();
        }
    }

    // RETURNING clause (RETURNING is contextual)
    if (matchContextual("RETURNING")) {
        parseReturningClause(stmt->returning);
    }

    stmt->span = makeSpan(start);
    return stmt;
}

InsertStmt* Parser::parseUpdateOrInsert() {
    SourceLocation start = currentLocation();
    ParseModeGuard guard(state_, ParseMode::DML_INSERT);

    expect(TokenType::KW_OR, "Expected OR after UPDATE");
    if (!(match(TokenType::KW_INSERT) || matchContextual("INSERT"))) {
        errorCode("PRS_0505", "Expected INSERT after UPDATE OR");
        return nullptr;
    }

    auto* stmt = arena_.create<InsertStmt>();
    match(TokenType::KW_INTO);
    stmt->table_path = parseSchemaPath(state_);
    if (stmt->table_path.isEmpty()) {
        errorCode("PRS_0505", "Expected target table in UPDATE OR INSERT");
        return stmt;
    }

    if (check(TokenType::LEFT_PAREN)) {
        parseInsertColumns(stmt);
    }

    if (!match(TokenType::KW_VALUES)) {
        errorCode("PRS_0505", "UPDATE OR INSERT requires VALUES clause");
        return stmt;
    }
    stmt->source = InsertStmt::Source::VALUES;
    parseValuesClause(stmt);

    if (matchContextual("MATCHING")) {
        expect(TokenType::LEFT_PAREN, "Expected '(' after MATCHING");
        auto* conflict = arena_.create<OnConflictClause>();
        do {
            conflict->columns.push_back(expectIdentifier("Expected MATCHING column"));
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after MATCHING columns");
        conflict->action = ConflictAction::UPDATE;
        for (auto column : stmt->columns) {
            auto* ref = arena_.create<ColumnRefExpr>();
            ref->column.column_name = column;
            ref->column.has_table_qualifier = true;
            conflict->set_items.push_back({column, ref});
        }
        stmt->on_conflict = conflict;
    } else if (!stmt->columns.empty()) {
        auto* conflict = arena_.create<OnConflictClause>();
        conflict->columns = stmt->columns;
        conflict->action = ConflictAction::UPDATE;
        for (auto column : stmt->columns) {
            auto* ref = arena_.create<ColumnRefExpr>();
            ref->column.column_name = column;
            ref->column.has_table_qualifier = true;
            conflict->set_items.push_back({column, ref});
        }
        stmt->on_conflict = conflict;
    }

    if (matchContextual("RETURNING")) {
        parseReturningClause(stmt->returning);
    }

    stmt->span = makeSpan(start);
    return stmt;
}

void Parser::parseInsertColumns(InsertStmt* stmt) {
    expect(TokenType::LEFT_PAREN, "Expected '(' before column list");
    do {
        stmt->columns.push_back(expectIdentifier("Expected column name"));
    } while (match(TokenType::COMMA));
    expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");
}

void Parser::parseValuesClause(InsertStmt* stmt) {
    // Parse multiple value rows
    do {
        std::vector<Expression*> row;
        expect(TokenType::LEFT_PAREN, "Expected '(' before values");
        do {
            if (match(TokenType::KW_DEFAULT)) {
                // DEFAULT value
                auto* expr = arena_.create<LiteralExpr>();
                expr->literal_type = LiteralType::DEFAULT;
                row.push_back(expr);
            } else {
                row.push_back(parseExpression());
            }
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after values");
        stmt->values_rows.push_back(std::move(row));
    } while (match(TokenType::COMMA));
}

void Parser::parseOnConflict(InsertStmt* stmt) {
    stmt->on_conflict = arena_.create<OnConflictClause>();

    // Conflict target (optional)
    if (check(TokenType::LEFT_PAREN)) {
        advance();
        do {
            stmt->on_conflict->columns.push_back(expectIdentifier("Expected column name"));
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after conflict columns");
    } else if (match(TokenType::KW_ON)) {
        expectContextual("CONSTRAINT", "Expected CONSTRAINT after ON");
        stmt->on_conflict->constraint_name = expectIdentifier("Expected constraint name");
    }

    // WHERE clause for partial index
    if (match(TokenType::KW_WHERE)) {
        stmt->on_conflict->where_target = parseExpression();
    }

    // DO action
    expectContextual("DO", "Expected DO after conflict target");

    if (matchContextual("NOTHING")) {
        stmt->on_conflict->action = ConflictAction::NOTHING;
    } else if (match(TokenType::KW_UPDATE)) {
        stmt->on_conflict->action = ConflictAction::UPDATE;
        expect(TokenType::KW_SET, "Expected SET after UPDATE");

        // Parse SET assignments
        do {
            StringPool::StringId column = expectIdentifier("Expected column name");
            expect(TokenType::EQUAL, "Expected '=' in SET clause");
            Expression* value = nullptr;
            if (matchContextual("EXCLUDED")) {
                expect(TokenType::DOT, "Expected '.' after EXCLUDED");
                // Create column reference to EXCLUDED.column
                auto* ref = arena_.create<ColumnRefExpr>();
                ref->column.column_name = expectIdentifier("Expected column name");
                ref->column.has_table_qualifier = true;
                // Use "EXCLUDED" as qualifier
                value = ref;
            } else {
                value = parseExpression();
            }
            stmt->on_conflict->set_items.push_back({column, value});
        } while (match(TokenType::COMMA));

        // Optional WHERE clause for UPDATE
        if (match(TokenType::KW_WHERE)) {
            stmt->on_conflict->where_action = parseExpression();
        }
    }
}

void Parser::parseConsistencyClause(StringPool::StringId& consistency_level,
                                    StringPool::StringId& serial_consistency_level) {
    if (!(match(TokenType::KW_WITH) || match(TokenType::KW_USING) ||
          matchContextual("WITH") || matchContextual("USING"))) {
        return;
    }

    auto parse_level = [&](const char* message) -> StringPool::StringId {
        if (!isIdentifier()) {
            errorCode("PRS_0504", message);
            return StringPool::INVALID_ID;
        }
        StringPool::StringId level = expectIdentifier(message);
        if (level == StringPool::INVALID_ID) {
            return level;
        }
        std::string normalized(stringPool().get(level));
        for (char& c : normalized) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        return stringPool().intern(normalized);
    };

    bool parsed_any = false;
    while (true) {
        if (matchContextual("CONSISTENCY")) {
            consistency_level = parse_level("Expected consistency level after CONSISTENCY");
            parsed_any = true;
        } else if (matchContextual("SERIAL")) {
            expectContextual("CONSISTENCY", "Expected CONSISTENCY after SERIAL");
            serial_consistency_level =
                parse_level("Expected serial consistency level after SERIAL CONSISTENCY");
            parsed_any = true;
        } else if (match(TokenType::KW_AND) || match(TokenType::COMMA)) {
            continue;
        } else {
            break;
        }
    }

    if (!parsed_any) {
        errorCode("PRS_0504", "Expected CONSISTENCY or SERIAL CONSISTENCY after WITH/USING");
    }
}

void Parser::parseConditionalWriteClause(bool allow_if_not_exists,
                                         bool& conditional_if_exists,
                                         bool& conditional_if_not_exists,
                                         Expression*& conditional_if) {
    auto parse_if_payload = [&]() {
        if (allow_if_not_exists && match(TokenType::KW_NOT)) {
            expect(TokenType::KW_EXISTS, "Expected EXISTS after IF/WHEN NOT");
            conditional_if_not_exists = true;
            return;
        }
        if (match(TokenType::KW_EXISTS)) {
            conditional_if_exists = true;
            return;
        }
        conditional_if = parseExpression();
    };

    if (match(TokenType::KW_IF)) {
        parse_if_payload();
        return;
    }
    if (matchContextual("WHEN")) {
        parse_if_payload();
    }
}

void Parser::parseReturningClause(std::vector<SelectItem*>& returning) {
    do {
        SelectItem* item = parseSelectItem();
        if (item) {
            returning.push_back(item);
        }
    } while (match(TokenType::COMMA));
}

// =============================================================================
// UPDATE Statement
// =============================================================================

UpdateStmt* Parser::parseUpdate() {
    SourceLocation start = currentLocation();
    ParseModeGuard guard(state_, ParseMode::DML_UPDATE);

    auto* stmt = arena_.create<UpdateStmt>();

    // ONLY modifier (contextual keyword)
    // Note: AST doesn't have 'only' field, skip for now

    // Table name
    stmt->table_path = parseSchemaPath(state_);

    // Optional alias
    if (match(TokenType::KW_AS)) {
        stmt->alias = expectIdentifier("Expected alias after AS");
        stmt->has_alias = true;
    } else if (isIdentifier() && !check(TokenType::KW_SET)) {
        stmt->alias = currentIdentifier();
        stmt->has_alias = true;
    }

    // SET clause
    expect(TokenType::KW_SET, "Expected SET in UPDATE statement");
    parseSetClause(stmt);

    // FROM clause (PostgreSQL extension)
    if (match(TokenType::KW_FROM)) {
        ParseModeGuard fromGuard(state_, ParseMode::TABLE_REF);
        stmt->from = parseTableRef();

        // Parse joins in FROM clause
        while (check(TokenType::KW_JOIN) ||
               checkContextual("LEFT") ||
               checkContextual("RIGHT") ||
               checkContextual("INNER") ||
               checkContextual("FULL") ||
               checkContextual("CROSS") ||
               checkContextual("NATURAL")) {
            JoinNode* join = parseJoin(stmt->from);
            if (join) {
                stmt->joins.push_back(join);
            }
        }
    }

    // WHERE clause
    if (match(TokenType::KW_WHERE)) {
        ParseModeGuard whereGuard(state_, ParseMode::EXPRESSION);
        // WHERE CURRENT OF is for cursors - not handling for now
        stmt->where = parseExpression();
    }

    if (match(TokenType::KW_ORDER)) {
        matchContextual("BY");
        if (!requireFeature(kFeatureDmlUpdateOrderLimit)) {
            while (!isAtEnd() &&
                   !checkContextual("RETURNING") &&
                   !check(TokenType::SEMICOLON) &&
                   !check(TokenType::END_OF_FILE)) {
                advance();
            }
            stmt->span = makeSpan(start);
            return stmt;
        }
        errorCode("PRS_0505",
                  "UPDATE ... ORDER BY ... LIMIT is not supported in v3; use canonical key-subquery rewrite");
        while (!isAtEnd() &&
               !check(TokenType::KW_LIMIT) &&
               !checkContextual("RETURNING") &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            advance();
        }
    }
    if (match(TokenType::KW_LIMIT)) {
        if (!requireFeature(kFeatureDmlUpdateOrderLimit)) {
            while (!isAtEnd() &&
                   !checkContextual("RETURNING") &&
                   !check(TokenType::SEMICOLON) &&
                   !check(TokenType::END_OF_FILE)) {
                advance();
            }
            stmt->span = makeSpan(start);
            return stmt;
        }
        errorCode("PRS_0505",
                  "UPDATE ... ORDER BY ... LIMIT is not supported in v3; use canonical key-subquery rewrite");
        if (!isAtEnd() &&
            !checkContextual("RETURNING") &&
            !check(TokenType::SEMICOLON) &&
            !check(TokenType::END_OF_FILE)) {
            advance();
        }
    }

    parseConsistencyClause(stmt->consistency_level, stmt->serial_consistency_level);
    bool ignored_if_not_exists = false;
    parseConditionalWriteClause(false,
                                stmt->conditional_if_exists,
                                ignored_if_not_exists,
                                stmt->conditional_if);

    if (matchContextual("OUTPUT")) {
        errorCode("PRS_0505", "OUTPUT clause is not supported in v3; use RETURNING");
        while (!isAtEnd() &&
               !checkContextual("RETURNING") &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            advance();
        }
    }

    // RETURNING clause (RETURNING is contextual)
    if (matchContextual("RETURNING")) {
        parseReturningClause(stmt->returning);
    }

    stmt->span = makeSpan(start);
    return stmt;
}

void Parser::parseSetClause(UpdateStmt* stmt) {
    do {
        // Single column assignment: col = expr
        StringPool::StringId column = expectIdentifier("Expected column name");
        expect(TokenType::EQUAL, "Expected '=' in SET clause");

        Expression* value = nullptr;
        if (match(TokenType::KW_DEFAULT)) {
            auto* expr = arena_.create<LiteralExpr>();
            expr->literal_type = LiteralType::DEFAULT;
            value = expr;
        } else {
            value = parseExpression();
        }

        stmt->set_items.push_back({column, value});
    } while (match(TokenType::COMMA));
}

// =============================================================================
// DELETE Statement
// =============================================================================

DeleteStmt* Parser::parseDelete() {
    SourceLocation start = currentLocation();
    ParseModeGuard guard(state_, ParseMode::DML_DELETE);

    auto* stmt = arena_.create<DeleteStmt>();

    // FROM keyword (required in standard SQL)
    expect(TokenType::KW_FROM, "Expected FROM in DELETE statement");

    // ONLY modifier (contextual keyword)
    // Note: AST doesn't have 'only' field, skip for now

    // Table name
    stmt->table_path = parseSchemaPath(state_);

    // Optional alias
    if (match(TokenType::KW_AS)) {
        stmt->alias = expectIdentifier("Expected alias after AS");
        stmt->has_alias = true;
    } else if (isIdentifier() &&
               !check(TokenType::KW_WHERE) &&
               !check(TokenType::KW_USING) &&
               !checkContextual("OUTPUT") &&
               !checkContextual("RETURNING")) {
        stmt->alias = currentIdentifier();
        stmt->has_alias = true;
    }

    // USING clause (PostgreSQL extension)
    if (match(TokenType::KW_USING)) {
        ParseModeGuard usingGuard(state_, ParseMode::TABLE_REF);
        stmt->using_clause = parseTableRef();

        // Parse joins in USING clause
        while (check(TokenType::KW_JOIN) ||
               checkContextual("LEFT") ||
               checkContextual("RIGHT") ||
               checkContextual("INNER") ||
               checkContextual("FULL") ||
               checkContextual("CROSS") ||
               checkContextual("NATURAL")) {
            JoinNode* join = parseJoin(stmt->using_clause);
            if (join) {
                stmt->using_joins.push_back(join);
            }
        }
    }

    // WHERE clause
    if (match(TokenType::KW_WHERE)) {
        ParseModeGuard whereGuard(state_, ParseMode::EXPRESSION);
        // WHERE CURRENT OF is for cursors - not handling for now
        stmt->where = parseExpression();
    }

    if (match(TokenType::KW_ORDER)) {
        matchContextual("BY");
        if (!requireFeature(kFeatureDmlDeleteOrderLimit)) {
            while (!isAtEnd() &&
                   !checkContextual("RETURNING") &&
                   !check(TokenType::SEMICOLON) &&
                   !check(TokenType::END_OF_FILE)) {
                advance();
            }
            stmt->span = makeSpan(start);
            return stmt;
        }
        errorCode("PRS_0505",
                  "DELETE ... ORDER BY ... LIMIT is not supported in v3; use canonical key-subquery rewrite");
        while (!isAtEnd() &&
               !check(TokenType::KW_LIMIT) &&
               !checkContextual("RETURNING") &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            advance();
        }
    }
    if (match(TokenType::KW_LIMIT)) {
        if (!requireFeature(kFeatureDmlDeleteOrderLimit)) {
            while (!isAtEnd() &&
                   !checkContextual("RETURNING") &&
                   !check(TokenType::SEMICOLON) &&
                   !check(TokenType::END_OF_FILE)) {
                advance();
            }
            stmt->span = makeSpan(start);
            return stmt;
        }
        errorCode("PRS_0505",
                  "DELETE ... ORDER BY ... LIMIT is not supported in v3; use canonical key-subquery rewrite");
        if (!isAtEnd() &&
            !checkContextual("RETURNING") &&
            !check(TokenType::SEMICOLON) &&
            !check(TokenType::END_OF_FILE)) {
            advance();
        }
    }

    parseConsistencyClause(stmt->consistency_level, stmt->serial_consistency_level);
    bool ignored_if_not_exists = false;
    parseConditionalWriteClause(false,
                                stmt->conditional_if_exists,
                                ignored_if_not_exists,
                                stmt->conditional_if);

    if (matchContextual("OUTPUT")) {
        errorCode("PRS_0505", "OUTPUT clause is not supported in v3; use RETURNING");
        while (!isAtEnd() &&
               !checkContextual("RETURNING") &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            advance();
        }
    }

    // RETURNING clause (RETURNING is contextual)
    if (matchContextual("RETURNING")) {
        parseReturningClause(stmt->returning);
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// COPY Statement
// =============================================================================

CopyStmt* Parser::parseCopy() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<CopyStmt>();

    // Spec: docs/specifications/parser/SCRATCHBIRD_SQL_COMPLETE_BNF.md <copy_command>

    bool has_query = false;
    if (match(TokenType::LEFT_PAREN)) {
        has_query = true;
        if (!match(TokenType::KW_SELECT)) {
            error("Expected SELECT after '(' in COPY");
            return nullptr;
        }
        stmt->query = parseSelect();
        expect(TokenType::RIGHT_PAREN, "Expected ')' after COPY query");
    } else {
        // Target table
        stmt->table_path = parseSchemaPath(state_);

        // Optional column list
        if (match(TokenType::LEFT_PAREN)) {
            do {
                stmt->columns.push_back(expectIdentifier("Expected column name"));
            } while (match(TokenType::COMMA));
            expect(TokenType::RIGHT_PAREN, "Expected ')' after COPY column list");
        }
    }

    // Direction
    if (match(TokenType::KW_FROM)) {
        stmt->direction = CopyStmt::Direction::FROM;
        if (matchContextual("PROGRAM")) {
            stmt->target_is_program = true;
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal after COPY FROM PROGRAM");
                return nullptr;
            }
            stmt->target = current().value.string_id;
            advance();
        } else if (matchContextual("STDIN")) {
            stmt->target_is_stdin = true;
        } else if (check(TokenType::STRING_LITERAL)) {
            stmt->target = current().value.string_id;
            advance();
        } else {
            error("Expected PROGRAM, STDIN, or string literal for COPY FROM");
            return nullptr;
        }
    } else if (matchContextual("TO")) {
        stmt->direction = CopyStmt::Direction::TO;
        if (matchContextual("PROGRAM")) {
            stmt->target_is_program = true;
            if (!check(TokenType::STRING_LITERAL)) {
                error("Expected string literal after COPY TO PROGRAM");
                return nullptr;
            }
            stmt->target = current().value.string_id;
            advance();
        } else if (matchContextual("STDOUT")) {
            stmt->target_is_stdout = true;
        } else if (check(TokenType::STRING_LITERAL)) {
            stmt->target = current().value.string_id;
            advance();
        } else {
            error("Expected PROGRAM, STDOUT, or string literal for COPY TO");
            return nullptr;
        }
    } else {
        error("Expected COPY FROM or COPY TO");
        return nullptr;
    }

    if (has_query && stmt->direction == CopyStmt::Direction::FROM) {
        error("COPY (SELECT ...) only supports TO");
        return nullptr;
    }

    // Optional WITH (...) options
    bool has_with = match(TokenType::KW_WITH);
    if (has_with || check(TokenType::LEFT_PAREN)) {
        if (!match(TokenType::LEFT_PAREN)) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after WITH in COPY");
        }

        auto read_option_value = [&]() -> StringPool::StringId {
            if (check(TokenType::STRING_LITERAL) || isIdentifier()) {
                auto id = current().value.string_id;
                advance();
                return id;
            }
            return StringPool::INVALID_ID;
        };

        auto parse_format = [&](StringPool::StringId id) {
            if (id == StringPool::INVALID_ID) {
                error("Expected COPY FORMAT value");
                return;
            }
            auto text = stringPool().get(id);
            if (caseInsensitiveEquals(text, "CSV")) {
                stmt->options.format = CopyOptions::Format::CSV;
                stmt->options.format_set = true;
            } else if (caseInsensitiveEquals(text, "TEXT")) {
                stmt->options.format = CopyOptions::Format::TEXT;
                stmt->options.format_set = true;
            } else if (caseInsensitiveEquals(text, "BINARY")) {
                stmt->options.format = CopyOptions::Format::BINARY;
                stmt->options.format_set = true;
            } else {
                error("Unsupported COPY FORMAT value");
            }
        };

        while (!check(TokenType::RIGHT_PAREN) &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            if (matchContextual("FORMAT")) {
                auto id = read_option_value();
                if (id == StringPool::INVALID_ID) {
                    error("Expected COPY FORMAT value");
                    break;
                }
                parse_format(id);
            } else if (matchContextual("DELIMITER")) {
                matchContextual("AS");
                if (!check(TokenType::STRING_LITERAL)) {
                    error("Expected string literal for COPY DELIMITER");
                    break;
                }
                stmt->options.delimiter = current().value.string_id;
                stmt->options.delimiter_set = true;
                advance();
            } else if (match(TokenType::KW_NULL)) {
                matchContextual("AS");
                if (!check(TokenType::STRING_LITERAL)) {
                    error("Expected string literal for COPY NULL");
                    break;
                }
                stmt->options.null_string = current().value.string_id;
                stmt->options.null_set = true;
                advance();
            } else if (matchContextual("HEADER")) {
                stmt->options.header = true;
                stmt->options.header_set = true;
                if (match(TokenType::KW_TRUE) || match(TokenType::KW_ON) || matchContextual("ON")) {
                    stmt->options.header = true;
                } else if (match(TokenType::KW_FALSE) || matchContextual("OFF")) {
                    stmt->options.header = false;
                }
            } else if (matchContextual("QUOTE")) {
                if (!check(TokenType::STRING_LITERAL)) {
                    error("Expected string literal for COPY QUOTE");
                    break;
                }
                stmt->options.quote = current().value.string_id;
                stmt->options.quote_set = true;
                advance();
            } else if (matchContextual("ESCAPE")) {
                if (!check(TokenType::STRING_LITERAL)) {
                    error("Expected string literal for COPY ESCAPE");
                    break;
                }
                stmt->options.escape = current().value.string_id;
                stmt->options.escape_set = true;
                advance();
            } else if (matchContextual("ENCODING")) {
                auto id = read_option_value();
                if (id == StringPool::INVALID_ID) {
                    error("Expected COPY ENCODING value");
                    break;
                }
                stmt->options.encoding = id;
                stmt->options.encoding_set = true;
            } else if (matchContextual("BATCH_SIZE")) {
                if (!check(TokenType::INTEGER_LITERAL)) {
                    error("Expected integer literal for COPY BATCH_SIZE");
                    break;
                }
                stmt->options.batch_size = current().value.int_value;
                stmt->options.batch_size_set = true;
                advance();
            } else if (matchContextual("MAX_ERRORS")) {
                if (!check(TokenType::INTEGER_LITERAL)) {
                    error("Expected integer literal for COPY MAX_ERRORS");
                    break;
                }
                stmt->options.max_errors = current().value.int_value;
                stmt->options.max_errors_set = true;
                advance();
            } else if (matchContextual("ON_ERROR")) {
                auto id = read_option_value();
                if (id == StringPool::INVALID_ID) {
                    error("Expected COPY ON_ERROR value");
                    break;
                }
                auto text = stringPool().get(id);
                if (caseInsensitiveEquals(text, "ABORT")) {
                    stmt->options.on_error = CopyOptions::OnError::ABORT;
                } else if (caseInsensitiveEquals(text, "SKIP")) {
                    stmt->options.on_error = CopyOptions::OnError::SKIP;
                } else {
                    error("Unsupported COPY ON_ERROR value");
                    break;
                }
                stmt->options.on_error_set = true;
            } else if (matchContextual("CSV") ||
                       matchContextual("TEXT") ||
                       matchContextual("BINARY")) {
                parse_format(previous().value.string_id);
            } else {
                error("Unsupported COPY option");
                break;
            }

            if (!match(TokenType::COMMA)) {
                break;
            }
        }

        expect(TokenType::RIGHT_PAREN, "Expected ')' after COPY options");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// Additional Expression Parsing for DML
// =============================================================================

Expression* Parser::parseCaseExpr() {
    auto* expr = arena_.create<CaseExpr>();
    SourceLocation start = currentLocation();

    // CASE already consumed

    // Simple CASE (CASE operand WHEN...) or searched CASE (CASE WHEN...)
    if (!check(TokenType::KW_WHEN)) {
        expr->operand = parseExpression();
    }

    // WHEN clauses
    while (match(TokenType::KW_WHEN)) {
        CaseExpr::WhenClause when;
        when.when_expr = parseExpression();
        expect(TokenType::KW_THEN, "Expected THEN after WHEN condition");
        when.then_expr = parseExpression();
        expr->when_clauses.push_back(when);
    }

    // ELSE clause
    if (match(TokenType::KW_ELSE)) {
        expr->else_expr = parseExpression();
    }

    expect(TokenType::KW_END, "Expected END after CASE expression");

    expr->span = makeSpan(start);
    return expr;
}

Expression* Parser::parseExistsExpr() {
    auto* expr = arena_.create<ExistsExpr>();
    SourceLocation start = currentLocation();

    // EXISTS already consumed
    expect(TokenType::LEFT_PAREN, "Expected '(' after EXISTS");

    if (check(TokenType::KW_SELECT) || check(TokenType::KW_WITH)) {
        expr->subquery = parseSelectWithClause();
    } else {
        error("Expected SELECT after EXISTS");
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after EXISTS subquery");

    expr->span = makeSpan(start);
    return expr;
}

Expression* Parser::parseInExpr(Expression* left) {
    auto* expr = arena_.create<InExpr>();
    SourceLocation start = currentLocation();

    expr->expr = left;

    // NOT IN was already handled, just IN here
    // IN already consumed

    expect(TokenType::LEFT_PAREN, "Expected '(' after IN");

    if (check(TokenType::KW_SELECT) || check(TokenType::KW_WITH)) {
        expr->subquery = parseSelectWithClause();
    } else {
        // List of values
        do {
            expr->values.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    expect(TokenType::RIGHT_PAREN, "Expected ')' after IN list");

    expr->span = makeSpan(start);
    return expr;
}

Expression* Parser::parseBetweenExpr(Expression* left) {
    auto* expr = arena_.create<BetweenExpr>();
    SourceLocation start = currentLocation();

    expr->expr = left;

    // BETWEEN already consumed
    // Parse at additive level (not AND level) so AND isn't consumed by logical expression
    expr->low = parseAddExpr();

    expect(TokenType::KW_AND, "Expected AND in BETWEEN expression");

    // Parse high bound at additive level as well
    expr->high = parseAddExpr();

    expr->span = makeSpan(start);
    return expr;
}

Expression* Parser::parseLikeExpr(Expression* left) {
    auto* expr = arena_.create<LikeExpr>();
    SourceLocation start = currentLocation();

    expr->expr = left;

    // LIKE already consumed
    expr->pattern = parseExpression();

    // ESCAPE clause
    if (matchContextual("ESCAPE")) {
        expr->escape = parseExpression();
    }

    expr->span = makeSpan(start);
    return expr;
}

Expression* Parser::parseIsNullExpr(Expression* left) {
    auto* expr = arena_.create<IsNullExpr>();
    SourceLocation start = currentLocation();

    expr->expr = left;
    // is_not is set by the caller

    expr->span = makeSpan(start);
    return expr;
}

Expression* Parser::parseArrayExpr() {
    auto* expr = arena_.create<LiteralArrayExpr>();
    SourceLocation start = currentLocation();

    // ARRAY already consumed
    expect(TokenType::LEFT_BRACKET, "Expected '[' after ARRAY");

    if (!check(TokenType::RIGHT_BRACKET)) {
        do {
            expr->elements.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    expect(TokenType::RIGHT_BRACKET, "Expected ']' after ARRAY elements");

    expr->dimensions = 1;
    expr->dim_lengths.push_back(static_cast<uint32_t>(expr->elements.size()));

    expr->span = makeSpan(start);
    return expr;
}

// =============================================================================
// Expression Parsing (Basic for DDL)
// =============================================================================

Expression* Parser::parseExpression() {
    SourceLocation start = currentLocation();
    Expression* expr = parseOrExpr();
    if (expr && expr->span.length == 0) {
        expr->span = makeSpan(start);
    }
    return expr;
}

Expression* Parser::parseExpressionWithLeft(Expression* left) {
    SourceLocation start = currentLocation();
    Expression* expr = parseOrExprWithLeft(left);
    if (expr && expr->span.length == 0) {
        expr->span = makeSpan(start);
    }
    return expr;
}

Expression* Parser::parseOrExpr() {
    Expression* left = parseAndExpr();

    while (match(TokenType::KW_OR)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::OR;
        expr->left = left;
        expr->right = parseAndExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseOrExprWithLeft(Expression* left) {
    left = parseAndExprWithLeft(left);

    while (match(TokenType::KW_OR)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::OR;
        expr->left = left;
        expr->right = parseAndExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseAndExpr() {
    Expression* left = parseNotExpr();

    while (match(TokenType::KW_AND)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::AND;
        expr->left = left;
        expr->right = parseNotExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseAndExprWithLeft(Expression* left) {
    left = parseComparisonExprWithLeft(left);

    while (match(TokenType::KW_AND)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::AND;
        expr->left = left;
        expr->right = parseNotExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseNotExpr() {
    if (match(TokenType::KW_NOT)) {
        auto* expr = arena_.create<UnaryExpr>();
        expr->op = UnaryOp::NOT;
        expr->operand = parseNotExpr();
        return expr;
    }

    return parseComparisonExpr();
}

Expression* Parser::parseComparisonExpr() {
    Expression* left = parseConcatExpr();

    // IS NULL / IS NOT NULL / IS TRUE / IS FALSE / IS UNKNOWN
    if (match(TokenType::KW_IS)) {
        bool is_not = match(TokenType::KW_NOT);

        if (match(TokenType::KW_NULL)) {
            auto* expr = arena_.create<IsNullExpr>();
            expr->expr = left;
            expr->negated = is_not;
            return expr;
        } else if (match(TokenType::KW_TRUE)) {
            auto* null_safe_eq = arena_.create<BinaryExpr>();
            null_safe_eq->op = BinaryOp::NULL_SAFE_EQ;
            null_safe_eq->left = left;
            auto* rhs = arena_.create<LiteralExpr>();
            rhs->literal_type = LiteralType::BOOLEAN;
            rhs->bool_value = true;
            null_safe_eq->right = rhs;
            if (!is_not) {
                return null_safe_eq;
            }
            auto* not_expr = arena_.create<UnaryExpr>();
            not_expr->op = UnaryOp::NOT;
            not_expr->operand = null_safe_eq;
            return not_expr;
        } else if (match(TokenType::KW_FALSE)) {
            auto* null_safe_eq = arena_.create<BinaryExpr>();
            null_safe_eq->op = BinaryOp::NULL_SAFE_EQ;
            null_safe_eq->left = left;
            auto* rhs = arena_.create<LiteralExpr>();
            rhs->literal_type = LiteralType::BOOLEAN;
            rhs->bool_value = false;
            null_safe_eq->right = rhs;
            if (!is_not) {
                return null_safe_eq;
            }
            auto* not_expr = arena_.create<UnaryExpr>();
            not_expr->op = UnaryOp::NOT;
            not_expr->operand = null_safe_eq;
            return not_expr;
        } else if (matchContextual("DISTINCT")) {
            // IS [NOT] DISTINCT FROM
            expect(TokenType::KW_FROM, "Expected FROM after DISTINCT");
            auto* null_safe_eq = arena_.create<BinaryExpr>();
            null_safe_eq->op = BinaryOp::NULL_SAFE_EQ;
            null_safe_eq->left = left;
            null_safe_eq->right = parseConcatExpr();
            if (is_not) {
                return null_safe_eq;
            }

            auto* not_expr = arena_.create<UnaryExpr>();
            not_expr->op = UnaryOp::NOT;
            not_expr->operand = null_safe_eq;
            return not_expr;
        } else if (matchContextual("UNKNOWN")) {
            auto* expr = arena_.create<IsNullExpr>();
            expr->expr = left;
            expr->negated = is_not;
            return expr;
        }

        error("Expected NULL, TRUE, FALSE, DISTINCT, or UNKNOWN after IS");
        return left;
    }

    // NOT IN / NOT BETWEEN / NOT LIKE
    if (match(TokenType::KW_NOT)) {
        if (match(TokenType::KW_IN)) {
            auto* expr = parseInExpr(left);
            if (auto* inExpr = dynamic_cast<InExpr*>(expr)) {
                inExpr->negated = true;
            }
            return expr;
        } else if (match(TokenType::KW_BETWEEN)) {
            auto* expr = parseBetweenExpr(left);
            if (auto* betweenExpr = dynamic_cast<BetweenExpr*>(expr)) {
                betweenExpr->negated = true;
            }
            return expr;
        } else if (match(TokenType::KW_LIKE)) {
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->match_kind = LikeMatchKind::LIKE;
            }
            return expr;
        } else if (match(TokenType::KW_STARTING)) {
            expect(TokenType::KW_WITH, "Expected WITH after STARTING");
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->match_kind = LikeMatchKind::STARTING;
            }
            return expr;
        } else if (match(TokenType::KW_CONTAINING)) {
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->match_kind = LikeMatchKind::CONTAINING;
            }
            return expr;
        } else if (matchContextual("ILIKE")) {
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->case_insensitive = true;
                likeExpr->match_kind = LikeMatchKind::ILIKE;
            }
            return expr;
        } else if (matchContextual("SIMILAR")) {
            expectContextual("TO", "Expected TO after SIMILAR");
            // SIMILAR TO is treated as LIKE with regex semantics
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->match_kind = LikeMatchKind::SIMILAR;
            }
            return expr;
        }
        error("Expected IN, BETWEEN, LIKE, STARTING, CONTAINING, ILIKE, or SIMILAR after NOT");
        return left;
    }

    // IN
    if (match(TokenType::KW_IN)) {
        return parseInExpr(left);
    }

    // BETWEEN
    if (match(TokenType::KW_BETWEEN)) {
        return parseBetweenExpr(left);
    }

    // LIKE
    if (match(TokenType::KW_LIKE)) {
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->match_kind = LikeMatchKind::LIKE;
        }
        return expr;
    }

    if (match(TokenType::KW_STARTING)) {
        expect(TokenType::KW_WITH, "Expected WITH after STARTING");
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->match_kind = LikeMatchKind::STARTING;
        }
        return expr;
    }

    if (match(TokenType::KW_CONTAINING)) {
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->match_kind = LikeMatchKind::CONTAINING;
        }
        return expr;
    }

    // ILIKE (PostgreSQL case-insensitive LIKE)
    if (matchContextual("ILIKE")) {
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->case_insensitive = true;
            likeExpr->match_kind = LikeMatchKind::ILIKE;
        }
        return expr;
    }

    // SIMILAR TO
    if (matchContextual("SIMILAR")) {
        expectContextual("TO", "Expected TO after SIMILAR");
        // SIMILAR TO is treated as LIKE with regex semantics
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->match_kind = LikeMatchKind::SIMILAR;
        }
        return expr;
    }

    if (match(TokenType::TILDE)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_MATCH;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }
    if (match(TokenType::TILDE_STAR)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_MATCH_CI;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }
    if (match(TokenType::EXCLAIM_TILDE)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_NOT_MATCH;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }
    if (match(TokenType::EXCLAIM_TILDE_STAR)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_NOT_MATCH_CI;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }

    if (match(TokenType::QUESTION_MARK)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::JSON_EXISTS;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }
    if (match(TokenType::QUESTION_PIPE)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::JSON_EXISTS_ANY;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }
    if (match(TokenType::QUESTION_AMPERSAND)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::JSON_EXISTS_ALL;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }

    if (match(TokenType::AT_GREATER)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::ARRAY_CONTAINS;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }
    if (match(TokenType::LESS_AT)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::ARRAY_CONTAINED_BY;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }
    if (match(TokenType::DOUBLE_AMPERSAND)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::ARRAY_OVERLAP;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }

    // Comparison operators
    BinaryOp op;
    bool found = false;

    if (match(TokenType::EQUAL)) { op = BinaryOp::EQ; found = true; }
    else if (match(TokenType::NOT_EQUAL)) { op = BinaryOp::NE; found = true; }
    else if (match(TokenType::LESS_THAN)) { op = BinaryOp::LT; found = true; }
    else if (match(TokenType::LESS_EQUAL)) { op = BinaryOp::LE; found = true; }
    else if (match(TokenType::GREATER_THAN)) { op = BinaryOp::GT; found = true; }
    else if (match(TokenType::GREATER_EQUAL)) { op = BinaryOp::GE; found = true; }

    if (found) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }

    return left;
}

Expression* Parser::parseComparisonExprWithLeft(Expression* left) {
    left = parseConcatExprWithLeft(left);

    if (match(TokenType::KW_IS)) {
        bool is_not = match(TokenType::KW_NOT);

        if (match(TokenType::KW_NULL)) {
            auto* expr = arena_.create<IsNullExpr>();
            expr->expr = left;
            expr->negated = is_not;
            return expr;
        } else if (match(TokenType::KW_TRUE)) {
            auto* null_safe_eq = arena_.create<BinaryExpr>();
            null_safe_eq->op = BinaryOp::NULL_SAFE_EQ;
            null_safe_eq->left = left;
            auto* rhs = arena_.create<LiteralExpr>();
            rhs->literal_type = LiteralType::BOOLEAN;
            rhs->bool_value = true;
            null_safe_eq->right = rhs;
            if (!is_not) {
                return null_safe_eq;
            }
            auto* not_expr = arena_.create<UnaryExpr>();
            not_expr->op = UnaryOp::NOT;
            not_expr->operand = null_safe_eq;
            return not_expr;
        } else if (match(TokenType::KW_FALSE)) {
            auto* null_safe_eq = arena_.create<BinaryExpr>();
            null_safe_eq->op = BinaryOp::NULL_SAFE_EQ;
            null_safe_eq->left = left;
            auto* rhs = arena_.create<LiteralExpr>();
            rhs->literal_type = LiteralType::BOOLEAN;
            rhs->bool_value = false;
            null_safe_eq->right = rhs;
            if (!is_not) {
                return null_safe_eq;
            }
            auto* not_expr = arena_.create<UnaryExpr>();
            not_expr->op = UnaryOp::NOT;
            not_expr->operand = null_safe_eq;
            return not_expr;
        } else if (matchContextual("DISTINCT")) {
            expect(TokenType::KW_FROM, "Expected FROM after DISTINCT");
            auto* null_safe_eq = arena_.create<BinaryExpr>();
            null_safe_eq->op = BinaryOp::NULL_SAFE_EQ;
            null_safe_eq->left = left;
            null_safe_eq->right = parseConcatExpr();
            if (is_not) {
                return null_safe_eq;
            }

            auto* not_expr = arena_.create<UnaryExpr>();
            not_expr->op = UnaryOp::NOT;
            not_expr->operand = null_safe_eq;
            return not_expr;
        } else if (matchContextual("UNKNOWN")) {
            auto* expr = arena_.create<IsNullExpr>();
            expr->expr = left;
            expr->negated = is_not;
            return expr;
        }

        error("Expected NULL, TRUE, FALSE, DISTINCT, or UNKNOWN after IS");
        return left;
    }

    if (match(TokenType::KW_NOT)) {
        if (match(TokenType::KW_IN)) {
            auto* expr = parseInExpr(left);
            if (auto* inExpr = dynamic_cast<InExpr*>(expr)) {
                inExpr->negated = true;
            }
            return expr;
        } else if (match(TokenType::KW_BETWEEN)) {
            auto* expr = parseBetweenExpr(left);
            if (auto* betweenExpr = dynamic_cast<BetweenExpr*>(expr)) {
                betweenExpr->negated = true;
            }
            return expr;
        } else if (match(TokenType::KW_LIKE)) {
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->match_kind = LikeMatchKind::LIKE;
            }
            return expr;
        } else if (match(TokenType::KW_STARTING)) {
            expect(TokenType::KW_WITH, "Expected WITH after STARTING");
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->match_kind = LikeMatchKind::STARTING;
            }
            return expr;
        } else if (match(TokenType::KW_CONTAINING)) {
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->match_kind = LikeMatchKind::CONTAINING;
            }
            return expr;
        } else if (matchContextual("ILIKE")) {
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->case_insensitive = true;
                likeExpr->match_kind = LikeMatchKind::ILIKE;
            }
            return expr;
        } else if (matchContextual("SIMILAR")) {
            expectContextual("TO", "Expected TO after SIMILAR");
            auto* expr = parseLikeExpr(left);
            if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
                likeExpr->negated = true;
                likeExpr->match_kind = LikeMatchKind::SIMILAR;
            }
            return expr;
        }
        error("Expected IN, BETWEEN, LIKE, STARTING, CONTAINING, ILIKE, or SIMILAR after NOT");
        return left;
    }

    if (match(TokenType::KW_IN)) {
        return parseInExpr(left);
    }

    if (match(TokenType::KW_BETWEEN)) {
        return parseBetweenExpr(left);
    }

    if (match(TokenType::KW_LIKE)) {
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->match_kind = LikeMatchKind::LIKE;
        }
        return expr;
    }

    if (match(TokenType::KW_STARTING)) {
        expect(TokenType::KW_WITH, "Expected WITH after STARTING");
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->match_kind = LikeMatchKind::STARTING;
        }
        return expr;
    }

    if (match(TokenType::KW_CONTAINING)) {
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->match_kind = LikeMatchKind::CONTAINING;
        }
        return expr;
    }

    if (matchContextual("ILIKE")) {
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->case_insensitive = true;
            likeExpr->match_kind = LikeMatchKind::ILIKE;
        }
        return expr;
    }

    if (matchContextual("SIMILAR")) {
        expectContextual("TO", "Expected TO after SIMILAR");
        auto* expr = parseLikeExpr(left);
        if (auto* likeExpr = dynamic_cast<LikeExpr*>(expr)) {
            likeExpr->match_kind = LikeMatchKind::SIMILAR;
        }
        return expr;
    }

    if (match(TokenType::TILDE)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_MATCH;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }
    if (match(TokenType::TILDE_STAR)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_MATCH_CI;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }
    if (match(TokenType::EXCLAIM_TILDE)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_NOT_MATCH;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }
    if (match(TokenType::EXCLAIM_TILDE_STAR)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::REGEX_NOT_MATCH_CI;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }

    if (match(TokenType::QUESTION_MARK)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::JSON_EXISTS;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }
    if (match(TokenType::QUESTION_PIPE)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::JSON_EXISTS_ANY;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }
    if (match(TokenType::QUESTION_AMPERSAND)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::JSON_EXISTS_ALL;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }

    if (match(TokenType::AT_GREATER)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::ARRAY_CONTAINS;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }
    if (match(TokenType::LESS_AT)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::ARRAY_CONTAINED_BY;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }
    if (match(TokenType::DOUBLE_AMPERSAND)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::ARRAY_OVERLAP;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }

    BinaryOp op;
    bool found = false;

    if (match(TokenType::EQUAL)) { op = BinaryOp::EQ; found = true; }
    else if (match(TokenType::NOT_EQUAL)) { op = BinaryOp::NE; found = true; }
    else if (match(TokenType::LESS_THAN)) { op = BinaryOp::LT; found = true; }
    else if (match(TokenType::LESS_EQUAL)) { op = BinaryOp::LE; found = true; }
    else if (match(TokenType::GREATER_THAN)) { op = BinaryOp::GT; found = true; }
    else if (match(TokenType::GREATER_EQUAL)) { op = BinaryOp::GE; found = true; }

    if (found) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = parseConcatExpr();
        return expr;
    }

    return left;
}

Expression* Parser::parseConcatExpr() {
    Expression* left = parseBitOrExpr();

    while (match(TokenType::DOUBLE_PIPE)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::CONCAT;
        expr->left = left;
        expr->right = parseBitOrExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseConcatExprWithLeft(Expression* left) {
    left = parseBitOrExprWithLeft(left);

    while (match(TokenType::DOUBLE_PIPE)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::CONCAT;
        expr->left = left;
        expr->right = parseBitOrExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseBitOrExpr() {
    Expression* left = parseBitXorExpr();

    while (match(TokenType::PIPE)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::BIT_OR;
        expr->left = left;
        expr->right = parseBitXorExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseBitOrExprWithLeft(Expression* left) {
    left = parseBitXorExprWithLeft(left);

    while (match(TokenType::PIPE)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::BIT_OR;
        expr->left = left;
        expr->right = parseBitXorExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseBitXorExpr() {
    Expression* left = parseBitAndExpr();

    while (match(TokenType::CARET)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::BIT_XOR;
        expr->left = left;
        expr->right = parseBitAndExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseBitXorExprWithLeft(Expression* left) {
    left = parseBitAndExprWithLeft(left);

    while (match(TokenType::CARET)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::BIT_XOR;
        expr->left = left;
        expr->right = parseBitAndExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseBitAndExpr() {
    Expression* left = parseShiftExpr();

    while (match(TokenType::AMPERSAND)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::BIT_AND;
        expr->left = left;
        expr->right = parseShiftExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseBitAndExprWithLeft(Expression* left) {
    left = parseShiftExprWithLeft(left);

    while (match(TokenType::AMPERSAND)) {
        auto* expr = arena_.create<BinaryExpr>();
        expr->op = BinaryOp::BIT_AND;
        expr->left = left;
        expr->right = parseShiftExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseShiftExpr() {
    Expression* left = parseAddExpr();

    while (true) {
        BinaryOp op;
        if (match(TokenType::SHIFT_LEFT)) op = BinaryOp::SHIFT_LEFT;
        else if (match(TokenType::SHIFT_RIGHT)) op = BinaryOp::SHIFT_RIGHT;
        else break;

        auto* expr = arena_.create<BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = parseAddExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseShiftExprWithLeft(Expression* left) {
    left = parseAddExprWithLeft(left);

    while (true) {
        BinaryOp op;
        if (match(TokenType::SHIFT_LEFT)) op = BinaryOp::SHIFT_LEFT;
        else if (match(TokenType::SHIFT_RIGHT)) op = BinaryOp::SHIFT_RIGHT;
        else break;

        auto* expr = arena_.create<BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = parseAddExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseAddExpr() {
    Expression* left = parseMulExpr();

    while (true) {
        BinaryOp op;
        if (match(TokenType::PLUS)) op = BinaryOp::ADD;
        else if (match(TokenType::MINUS)) op = BinaryOp::SUB;
        else break;

        auto* expr = arena_.create<BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = parseMulExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseAddExprWithLeft(Expression* left) {
    left = parseMulExprWithLeft(left);

    while (true) {
        BinaryOp op;
        if (match(TokenType::PLUS)) op = BinaryOp::ADD;
        else if (match(TokenType::MINUS)) op = BinaryOp::SUB;
        else break;

        auto* expr = arena_.create<BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = parseMulExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseMulExpr() {
    Expression* left = parsePowerExpr();

    while (true) {
        BinaryOp op;
        if (match(TokenType::STAR)) op = BinaryOp::MUL;
        else if (match(TokenType::SLASH)) op = BinaryOp::DIV;
        else if (match(TokenType::KW_DIV)) op = BinaryOp::DIV_INT;
        else if (match(TokenType::PERCENT)) op = BinaryOp::MOD;
        else if (match(TokenType::ARROW)) op = BinaryOp::JSON_EXTRACT;
        else if (match(TokenType::DOUBLE_ARROW)) op = BinaryOp::JSON_EXTRACT_TEXT;
        else if (match(TokenType::HASH_ARROW)) op = BinaryOp::JSON_HASH_EXTRACT;
        else if (match(TokenType::HASH_DOUBLE_ARROW)) op = BinaryOp::JSON_HASH_EXTRACT_TEXT;
        else break;

        auto* expr = arena_.create<BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = parsePowerExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parseMulExprWithLeft(Expression* left) {
    while (true) {
        BinaryOp op;
        if (match(TokenType::STAR)) op = BinaryOp::MUL;
        else if (match(TokenType::SLASH)) op = BinaryOp::DIV;
        else if (match(TokenType::KW_DIV)) op = BinaryOp::DIV_INT;
        else if (match(TokenType::PERCENT)) op = BinaryOp::MOD;
        else if (match(TokenType::ARROW)) op = BinaryOp::JSON_EXTRACT;
        else if (match(TokenType::DOUBLE_ARROW)) op = BinaryOp::JSON_EXTRACT_TEXT;
        else if (match(TokenType::HASH_ARROW)) op = BinaryOp::JSON_HASH_EXTRACT;
        else if (match(TokenType::HASH_DOUBLE_ARROW)) op = BinaryOp::JSON_HASH_EXTRACT_TEXT;
        else break;

        auto* expr = arena_.create<BinaryExpr>();
        expr->op = op;
        expr->left = left;
        expr->right = parsePowerExpr();
        left = expr;
    }

    return left;
}

Expression* Parser::parsePowerExpr() {
    return parseUnaryExpr();
}

Expression* Parser::parseUnaryExpr() {
    if (match(TokenType::PLUS)) {
        return parseUnaryExpr();
    }
    if (match(TokenType::TILDE)) {
        auto* expr = arena_.create<UnaryExpr>();
        expr->op = UnaryOp::BIT_NOT;
        expr->operand = parseUnaryExpr();
        return expr;
    }
    if (match(TokenType::MINUS)) {
        auto* expr = arena_.create<UnaryExpr>();
        expr->op = UnaryOp::NEGATE;
        expr->operand = parseUnaryExpr();
        return expr;
    }

    Expression* expr = parsePrimaryExpr();
    while (expr && match(TokenType::DOUBLE_COLON)) {
        auto* cast = arena_.create<CastExpr>();
        cast->expr = expr;
        cast->target_type = parseTypeName();
        expr = cast;
    }
    return expr;
}

Expression* Parser::parsePrimaryExpr() {
    Expression* expr = nullptr;

    if (check(TokenType::LEFT_BRACE)) {
        errorCode("PRS_0505",
                  "JDBC escape blocks ({fn ...}, {d ...}, {ts ...}) are not supported in v3; use canonical SQL forms");
        advance();
        while (!isAtEnd() &&
               !check(TokenType::RIGHT_BRACE) &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::COMMA) &&
               !check(TokenType::RIGHT_PAREN) &&
               !check(TokenType::END_OF_FILE)) {
            advance();
        }
        match(TokenType::RIGHT_BRACE);
        return nullptr;
    }

    // Literals
    if (check(TokenType::INTEGER_LITERAL) || check(TokenType::FLOAT_LITERAL) ||
        check(TokenType::STRING_LITERAL) || check(TokenType::BLOB_LITERAL) ||
        check(TokenType::KW_TRUE) || check(TokenType::KW_FALSE) ||
        check(TokenType::KW_NULL)) {
        expr = parseLiteral();
    }

    auto parseTypedLiteral = [&](const std::string& type_name,
                                 bool allow_time_zone) -> Expression* {
        TypeName type;
        type.name = stringPool().intern(type_name);
        if (allow_time_zone) {
            if (check(TokenType::KW_WITH)) {
                Token next = state_.lexer().peekToken();
                if (next.type == TokenType::IDENTIFIER &&
                    caseInsensitiveEquals(stringPool().get(next.value.string_id), "TIME")) {
                    match(TokenType::KW_WITH);
                    expectContextual("TIME", "Expected TIME after WITH");
                    expectContextual("ZONE", "Expected ZONE after WITH TIME");
                    type.with_time_zone = true;
                }
            } else if (checkContextual("WITHOUT")) {
                Token next = state_.lexer().peekToken();
                if (next.type == TokenType::IDENTIFIER &&
                    caseInsensitiveEquals(stringPool().get(next.value.string_id), "TIME")) {
                    matchContextual("WITHOUT");
                    expectContextual("TIME", "Expected TIME after WITHOUT");
                    matchContextual("ZONE");
                    type.with_time_zone = false;
                }
            }
        }

        if (!check(TokenType::STRING_LITERAL)) {
            error("Expected string literal after typed literal");
            return nullptr;
        }
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = current().value.string_id;
        advance();

        auto* cast = arena_.create<CastExpr>();
        cast->expr = lit;
        cast->target_type = type;
        return cast;
    };
    auto parseStringLiteralId = [&]() -> StringPool::StringId {
        if (!check(TokenType::STRING_LITERAL)) {
            error("Expected string literal after typed literal");
            return StringPool::INVALID_ID;
        }
        auto id = current().value.string_id;
        advance();
        return id;
    };
    auto parseInt64FromString = [&](StringPool::StringId id, int64_t& out) -> bool {
        if (id == StringPool::INVALID_ID) return false;
        try {
            out = std::stoll(std::string(stringPool().get(id)));
            return true;
        } catch (...) {
            return false;
        }
    };
    auto parseUInt64FromString = [&](StringPool::StringId id, uint64_t& out) -> bool {
        if (id == StringPool::INVALID_ID) return false;
        try {
            out = std::stoull(std::string(stringPool().get(id)));
            return true;
        } catch (...) {
            return false;
        }
    };

    auto next_is_string = [&]() -> bool {
        return state_.lexer().peekToken().type == TokenType::STRING_LITERAL;
    };
    auto next_is_with = [&]() -> bool {
        Token next = state_.lexer().peekToken();
        return next.type == TokenType::KW_WITH ||
               (next.type == TokenType::IDENTIFIER &&
                caseInsensitiveEquals(stringPool().get(next.value.string_id), "WITHOUT"));
    };

    if (!expr && checkContextual("DATE") && next_is_string()) {
        matchContextual("DATE");
        expr = parseTypedLiteral("DATE", false);
    }
    if (!expr && checkContextual("TIME") && (next_is_string() || next_is_with())) {
        matchContextual("TIME");
        expr = parseTypedLiteral("TIME", true);
    }
    if (!expr && checkContextual("TIMESTAMP") && (next_is_string() || next_is_with())) {
        matchContextual("TIMESTAMP");
        expr = parseTypedLiteral("TIMESTAMP", true);
    }
    if (!expr && checkContextual("UUID") && next_is_string()) {
        matchContextual("UUID");
        expr = parseTypedLiteral("UUID", false);
    }
    if (!expr && checkContextual("JSONPATH") && next_is_string()) {
        matchContextual("JSONPATH");
        auto* lit = arena_.create<LiteralJsonPathExpr>();
        lit->text = parseStringLiteralId();
        expr = lit;
    }
    if (!expr && checkContextual("ENUM") && next_is_string()) {
        matchContextual("ENUM");
        auto* lit = arena_.create<LiteralEnumExpr>();
        lit->label = parseStringLiteralId();
        lit->has_label = lit->label != StringPool::INVALID_ID;
        expr = lit;
    }
    if (!expr && checkContextual("SET") && state_.lexer().peekToken().type == TokenType::LEFT_BRACKET) {
        matchContextual("SET");
        expect(TokenType::LEFT_BRACKET, "Expected '[' after SET");
        auto* set = arena_.create<LiteralSetExpr>();
        if (!check(TokenType::RIGHT_BRACKET)) {
            do {
                auto* elem = arena_.create<LiteralEnumExpr>();
                if (check(TokenType::STRING_LITERAL)) {
                    elem->label = parseStringLiteralId();
                    elem->has_label = true;
                } else if (isIdentifier()) {
                    elem->label = currentIdentifier();
                    elem->has_label = true;
                    advance();
                } else {
                    error("Expected enum label in SET literal");
                }
                set->elements.push_back(elem);
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_BRACKET, "Expected ']' after SET literal");
        expr = set;
    }
    if (!expr && checkContextual("ROW") && state_.lexer().peekToken().type == TokenType::LEFT_PAREN) {
        matchContextual("ROW");
        expect(TokenType::LEFT_PAREN, "Expected '(' after ROW");
        auto* row = arena_.create<LiteralRowExpr>();
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                RowFieldLiteral field;
                field.value = parseExpression();
                row->fields.push_back(field);
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after ROW literal");
        expr = row;
    }
    if (!expr && checkContextual("COMPOSITE") && state_.lexer().peekToken().type == TokenType::LEFT_PAREN) {
        matchContextual("COMPOSITE");
        expect(TokenType::LEFT_PAREN, "Expected '(' after COMPOSITE");
        auto* comp = arena_.create<LiteralCompositeExpr>();
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                RowFieldLiteral field;
                field.value = parseExpression();
                comp->fields.push_back(field);
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after COMPOSITE literal");
        expr = comp;
    }
    if (!expr && checkContextual("DOMAIN") && isIdentifier()) {
        matchContextual("DOMAIN");
        parseSchemaPath(state_);
        auto* dom = arena_.create<LiteralDomainExpr>();
        if (check(TokenType::STRING_LITERAL) || check(TokenType::INTEGER_LITERAL) ||
            check(TokenType::FLOAT_LITERAL) || check(TokenType::KW_NULL) ||
            check(TokenType::KW_TRUE) || check(TokenType::KW_FALSE)) {
            dom->value = parseLiteral();
        } else {
            dom->value = parseExpression();
        }
        expr = dom;
    }
    if (!expr && checkContextual("TSVECTOR") && next_is_string()) {
        matchContextual("TSVECTOR");
        auto* lit = arena_.create<LiteralTsVectorExpr>();
        lit->text = parseStringLiteralId();
        expr = lit;
    }
    if (!expr && checkContextual("TSQUERY") && next_is_string()) {
        matchContextual("TSQUERY");
        auto* lit = arena_.create<LiteralTsQueryExpr>();
        lit->text = parseStringLiteralId();
        expr = lit;
    }
    if (!expr && checkContextual("YEAR") && next_is_string()) {
        matchContextual("YEAR");
        auto* lit = arena_.create<LiteralYearExpr>();
        auto id = parseStringLiteralId();
        int64_t v = 0;
        if (parseInt64FromString(id, v)) {
            lit->value = static_cast<int32_t>(v);
            std::string_view raw = stringPool().get(id);
            lit->format = raw.size() <= 2 ? 0 : 1;
        }
        expr = lit;
    }
    if (!expr && checkContextual("MEDIUMINT") && next_is_string()) {
        matchContextual("MEDIUMINT");
        auto* lit = arena_.create<LiteralMediumIntExpr>();
        auto id = parseStringLiteralId();
        int64_t v = 0;
        if (parseInt64FromString(id, v)) {
            lit->value = static_cast<int32_t>(v);
        }
        expr = lit;
    }
    if (!expr && checkContextual("INT8") && next_is_string()) {
        matchContextual("INT8");
        auto* lit = arena_.create<LiteralInt8Expr>();
        auto id = parseStringLiteralId();
        int64_t v = 0;
        if (parseInt64FromString(id, v)) lit->value = static_cast<int8_t>(v);
        expr = lit;
    }
    if (!expr && checkContextual("INT16") && next_is_string()) {
        matchContextual("INT16");
        auto* lit = arena_.create<LiteralInt16Expr>();
        auto id = parseStringLiteralId();
        int64_t v = 0;
        if (parseInt64FromString(id, v)) lit->value = static_cast<int16_t>(v);
        expr = lit;
    }
    if (!expr && checkContextual("UINT8") && next_is_string()) {
        matchContextual("UINT8");
        auto* lit = arena_.create<LiteralUInt8Expr>();
        auto id = parseStringLiteralId();
        uint64_t v = 0;
        if (parseUInt64FromString(id, v)) lit->value = static_cast<uint8_t>(v);
        expr = lit;
    }
    if (!expr && checkContextual("UINT16") && next_is_string()) {
        matchContextual("UINT16");
        auto* lit = arena_.create<LiteralUInt16Expr>();
        auto id = parseStringLiteralId();
        uint64_t v = 0;
        if (parseUInt64FromString(id, v)) lit->value = static_cast<uint16_t>(v);
        expr = lit;
    }
    if (!expr && checkContextual("UINT32") && next_is_string()) {
        matchContextual("UINT32");
        auto* lit = arena_.create<LiteralUInt32Expr>();
        auto id = parseStringLiteralId();
        uint64_t v = 0;
        if (parseUInt64FromString(id, v)) lit->value = static_cast<uint32_t>(v);
        expr = lit;
    }
    if (!expr && checkContextual("UINT64") && next_is_string()) {
        matchContextual("UINT64");
        auto* lit = arena_.create<LiteralUInt64Expr>();
        auto id = parseStringLiteralId();
        uint64_t v = 0;
        if (parseUInt64FromString(id, v)) lit->value = v;
        expr = lit;
    }
    if (!expr && checkContextual("UINT128") && next_is_string()) {
        matchContextual("UINT128");
        auto* lit = arena_.create<LiteralUInt128Expr>();
        auto id = parseStringLiteralId();
        scratchbird::core::uint128_t v = 0;
        if (parseUnsigned128(stringPool().get(id), v)) {
            storeU128LE(v, lit->value);
        }
        expr = lit;
    }
    if (!expr && checkContextual("INT128") && next_is_string()) {
        matchContextual("INT128");
        auto* lit = arena_.create<LiteralInt128Expr>();
        auto id = parseStringLiteralId();
        scratchbird::core::uint128_t v = 0;
        if (parseSigned128(stringPool().get(id), v)) {
            storeU128LE(v, lit->value);
        }
        expr = lit;
    }
    if (!expr && checkContextual("FLOAT32") && next_is_string()) {
        matchContextual("FLOAT32");
        auto* lit = arena_.create<LiteralFloat32Expr>();
        auto id = parseStringLiteralId();
        try {
            lit->value = std::stof(std::string(stringPool().get(id)));
        } catch (...) {
            lit->value = 0.0f;
        }
        expr = lit;
    }
    if (!expr && checkContextual("GEOMETRY") && next_is_string()) {
        matchContextual("GEOMETRY");
        auto* lit = arena_.create<LiteralGeometryExpr>();
        auto id = parseStringLiteralId();
        std::string_view raw = stringPool().get(id);
        lit->bytes.assign(raw.begin(), raw.end());
        expr = lit;
    }
    if (!expr && checkContextual("VARIANT") && next_is_string()) {
        matchContextual("VARIANT");
        auto* lit = arena_.create<LiteralVariantExpr>();
        lit->value = nullptr;
        auto id = parseStringLiteralId();
        auto* inner = arena_.create<LiteralExpr>();
        inner->literal_type = LiteralType::STRING;
        inner->string_value = id;
        lit->value = inner;
        expr = lit;
    }
    if (!expr && checkContextual("RANGE") &&
        (state_.lexer().peekToken().type == TokenType::LEFT_BRACKET ||
         state_.lexer().peekToken().type == TokenType::LEFT_PAREN ||
         next_is_string())) {
        matchContextual("RANGE");
        auto* lit = arena_.create<LiteralRangeExpr>();
        if (next_is_string()) {
            parseStringLiteralId();
            expr = lit;
        } else {
            bool lower_inc = false;
            bool upper_inc = false;
            if (match(TokenType::LEFT_BRACKET)) lower_inc = true;
            else expect(TokenType::LEFT_PAREN, "Expected '[' or '(' after RANGE");
            if (!check(TokenType::COMMA)) {
                lit->lower_present = true;
                lit->lower = parseExpression();
            }
            expect(TokenType::COMMA, "Expected ',' in RANGE literal");
            if (!check(TokenType::RIGHT_BRACKET) && !check(TokenType::RIGHT_PAREN)) {
                lit->upper_present = true;
                lit->upper = parseExpression();
            }
            if (match(TokenType::RIGHT_BRACKET)) upper_inc = true;
            else expect(TokenType::RIGHT_PAREN, "Expected ')' or ']' after RANGE");
            if (lower_inc) lit->flags |= 0x01;
            if (upper_inc) lit->flags |= 0x02;
            if (!lit->lower_present) lit->flags |= 0x04;
            if (!lit->upper_present) lit->flags |= 0x08;
            expr = lit;
        }
    }
    if (!expr && checkContextual("BLOB_LOCATOR") &&
        (state_.lexer().peekToken().type == TokenType::LEFT_PAREN || next_is_string())) {
        matchContextual("BLOB_LOCATOR");
        auto* lit = arena_.create<LiteralBlobLocatorExpr>();
        if (match(TokenType::LEFT_PAREN)) {
            auto id = parseStringLiteralId();
            parseUuidBytes(stringPool().get(id), lit->blob_id);
            if (match(TokenType::COMMA)) {
                if (check(TokenType::INTEGER_LITERAL)) {
                    lit->blob_subtype = static_cast<int16_t>(current().value.int_value);
                    advance();
                }
            }
            if (match(TokenType::COMMA)) {
                if (check(TokenType::INTEGER_LITERAL)) {
                    lit->blob_length = static_cast<uint64_t>(current().value.int_value);
                    advance();
                }
            }
            if (match(TokenType::COMMA)) {
                if (check(TokenType::INTEGER_LITERAL)) {
                    lit->compression = static_cast<uint8_t>(current().value.int_value);
                    advance();
                }
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after BLOB_LOCATOR");
        } else {
            auto id = parseStringLiteralId();
            parseUuidBytes(stringPool().get(id), lit->blob_id);
        }
        expr = lit;
    }
    if (!expr && checkContextual("BIT") && next_is_string()) {
        matchContextual("BIT");
        auto* lit = arena_.create<LiteralBitExpr>();
        auto id = parseStringLiteralId();
        std::string_view raw = stringPool().get(id);
        lit->bit_length = static_cast<uint16_t>(raw.size());
        lit->bytes.assign(raw.begin(), raw.end());
        expr = lit;
    }
    if (!expr && checkContextual("DATETIME") && next_is_string()) {
        matchContextual("DATETIME");
        auto* lit = arena_.create<LiteralDateTimeExpr>();
        auto id = parseStringLiteralId();
        ParsedTimestampTz parsed;
        if (parseTimestampTz(stringPool().get(id), parsed)) {
            lit->epoch_usec = parsed.epoch_usec;
            lit->with_timezone = false;
            lit->precision = 0;
        }
        expr = lit;
    }
    if (!expr && checkContextual("TIME_TZ") && next_is_string()) {
        matchContextual("TIME_TZ");
        auto* lit = arena_.create<LiteralTimeTzExpr>();
        auto id = parseStringLiteralId();
        ParsedTimeTz parsed;
        if (parseTimeTz(stringPool().get(id), parsed)) {
            lit->time_usec = parsed.time_usec;
            lit->tz_offset_minutes = parsed.offset_minutes;
            if (!parsed.tz_name.empty()) {
                lit->tz_name = stringPool().intern(parsed.tz_name);
            }
        }
        expr = lit;
    }
    if (!expr && checkContextual("TIMESTAMP_TZ") && next_is_string()) {
        matchContextual("TIMESTAMP_TZ");
        auto* lit = arena_.create<LiteralTimestampTzExpr>();
        auto id = parseStringLiteralId();
        ParsedTimestampTz parsed;
        if (parseTimestampTz(stringPool().get(id), parsed)) {
            lit->epoch_usec = parsed.epoch_usec;
            lit->tz_offset_minutes = parsed.offset_minutes;
            if (!parsed.tz_name.empty()) {
                lit->tz_name = stringPool().intern(parsed.tz_name);
            }
        }
        expr = lit;
    }

    if (!expr && matchContextual("EXTRACT")) {
        expr = parseExtractExpr();
    }

    if (!expr && matchContextual("ALTER_ELEMENT")) {
        expr = parseAlterElementExpr();
    }

    if (!expr && check(TokenType::PARAMETER)) {
        auto* param = arena_.create<ParameterExpr>();
        std::string_view text = state_.getTokenText(current());
        if (!text.empty() && text.front() == ':') {
            param->is_named = true;
            if (current().value.string_id != StringPool::INVALID_ID) {
                param->name = current().value.string_id;
            } else {
                param->name = stringPool().intern(text.substr(1));
            }
        } else {
            param->index = current().value.param_index;
        }
        advance();
        expr = param;
    }

    // CAST expression
    if (!expr && match(TokenType::KW_CAST)) {
        expr = parseCastExpr();
    }

    // CASE expression
    if (!expr && match(TokenType::KW_CASE)) {
        expr = parseCaseExpr();
    }

    // EXISTS expression
    if (!expr && match(TokenType::KW_EXISTS)) {
        expr = parseExistsExpr();
    }

    // ARRAY expression
    if (!expr && matchContextual("ARRAY")) {
        expr = parseArrayExpr();
    }

    // Parenthesized expression or subquery
    if (!expr && check(TokenType::LEFT_PAREN)) {
        advance();
        if (check(TokenType::KW_SELECT) || check(TokenType::KW_WITH)) {
            // Scalar subquery
            auto* subq = arena_.create<SubqueryExpr>();
            subq->subquery = parseSelectWithClause();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after subquery");
            expr = subq;
        } else {
            // Regular parenthesized expression
            Expression* inner = parseExpression();
            expect(TokenType::RIGHT_PAREN, "Expected ')'");
            expr = inner;
        }
    }

    auto parseContextFunction = [&](const char* keyword, const char* function_name) {
        if (expr || !matchContextual(keyword)) {
            return;
        }
        auto* fn = arena_.create<FunctionCallExpr>();
        fn->function_path.components.push_back(stringPool().intern(function_name));
        if (match(TokenType::LEFT_PAREN)) {
            if (!match(TokenType::RIGHT_PAREN)) {
                error(std::string(function_name).append(" does not accept arguments"));
                while (!isAtEnd() && !check(TokenType::RIGHT_PAREN) &&
                       !check(TokenType::SEMICOLON)) {
                    advance();
                }
                match(TokenType::RIGHT_PAREN);
            }
        }
        expr = fn;
    };
    parseContextFunction("CURRENT_USER", "CURRENT_USER");
    parseContextFunction("SESSION_USER", "SESSION_USER");
    parseContextFunction("CURRENT_ROLE", "CURRENT_ROLE");
    parseContextFunction("CURRENT_CONNECTION", "CURRENT_CONNECTION");
    parseContextFunction("CURRENT_SESSION", "CURRENT_CONNECTION");
    parseContextFunction("CURRENT_TRANSACTION", "CURRENT_TRANSACTION");
    parseContextFunction("NOW", "NOW");
    parseContextFunction("CURRENT_DATE", "CURRENT_DATE");
    parseContextFunction("CURRENT_TIME", "CURRENT_TIME");
    parseContextFunction("CURRENT_TIMESTAMP", "CURRENT_TIMESTAMP");

    // Column reference or function call
    if (!expr && (isIdentifier() || check(TokenType::DOT) || check(TokenType::DOUBLE_DOT))) {
        SchemaPath path = parseSchemaPath(state_);

        // Check for function call
        if (check(TokenType::LEFT_PAREN)) {
            expr = parseFunctionCall(std::move(path));
        } else {
            // Column reference
            auto* col = arena_.create<ColumnRefExpr>();
            if (path.components.size() == 1) {
                col->column.column_name = path.components[0];
            } else {
                col->column.column_name = path.objectName();
                col->column.has_table_qualifier = true;
                col->column.table_path.type = path.type;
                col->column.table_path.components = path.schemaComponents();
            }
            expr = col;
        }
    }

    if (!expr) {
        error("Expected expression");
        return nullptr;
    }

    auto makeFunctionCall = [&](std::string_view name, std::vector<Expression*> args) -> FunctionCallExpr* {
        auto* fn = arena_.create<FunctionCallExpr>();
        fn->function_path.components.push_back(stringPool().intern(name));
        fn->arguments = std::move(args);
        return fn;
    };

    auto makeNullLiteral = [&]() -> LiteralExpr* {
        auto* null_lit = arena_.create<LiteralExpr>();
        null_lit->literal_type = LiteralType::NULL_VALUE;
        return null_lit;
    };

    while (match(TokenType::LEFT_BRACKET)) {
        Expression* lower = nullptr;
        Expression* upper = nullptr;
        bool is_slice = false;

        if (!check(TokenType::COLON) && !check(TokenType::RIGHT_BRACKET)) {
            lower = parseExpression();
        }

        if (match(TokenType::COLON)) {
            is_slice = true;
            if (!check(TokenType::RIGHT_BRACKET)) {
                upper = parseExpression();
            }
        }

        expect(TokenType::RIGHT_BRACKET, "Expected ']' after array subscript");

        if (is_slice) {
            std::vector<Expression*> args;
            args.push_back(expr);
            args.push_back(lower ? lower : makeNullLiteral());
            args.push_back(upper ? upper : makeNullLiteral());
            expr = makeFunctionCall("ARRAY_SLICE", std::move(args));
        } else {
            if (!lower) {
                error("Expected array subscript expression");
                return expr;
            }
            std::vector<Expression*> args;
            args.push_back(expr);
            args.push_back(lower);
            expr = makeFunctionCall("ARRAY_SUBSCRIPT", std::move(args));
        }
    }

    return expr;
}

Expression* Parser::parseLiteral() {
    auto* expr = arena_.create<LiteralExpr>();

    if (check(TokenType::INTEGER_LITERAL)) {
        expr->literal_type = LiteralType::INTEGER;
        expr->int_value = current().value.int_value;
        advance();
    } else if (check(TokenType::FLOAT_LITERAL)) {
        expr->literal_type = LiteralType::FLOAT;
        expr->float_value = current().value.float_value;
        advance();
    } else if (check(TokenType::STRING_LITERAL)) {
        expr->literal_type = LiteralType::STRING;
        expr->string_value = current().value.string_id;
        advance();
    } else if (check(TokenType::BLOB_LITERAL)) {
        expr->literal_type = LiteralType::BLOB;
        expr->string_value = current().value.string_id;
        advance();
    } else if (match(TokenType::KW_TRUE)) {
        expr->literal_type = LiteralType::BOOLEAN;
        expr->bool_value = true;
    } else if (match(TokenType::KW_FALSE)) {
        expr->literal_type = LiteralType::BOOLEAN;
        expr->bool_value = false;
    } else if (match(TokenType::KW_NULL)) {
        expr->literal_type = LiteralType::NULL_VALUE;
    }

    return expr;
}

Expression* Parser::parseFunctionCall(SchemaPath path) {
    auto* expr = arena_.create<FunctionCallExpr>();
    expr->function_path = std::move(path);

    expect(TokenType::LEFT_PAREN, "Expected '(' for function call");

    std::string upper_name;
    if (!expr->function_path.components.empty())
    {
        std::string_view func_name = stringPool().get(expr->function_path.components.back());
        upper_name.reserve(func_name.size());
        for (char c : func_name)
        {
            upper_name.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }

    bool parsed_count_star = false;

    // Special-case COUNT(*)
    if (check(TokenType::STAR)) {
        if (upper_name != "COUNT") {
            error("'*' is only allowed in COUNT(*)");
        }

        // COUNT(*) is equivalent to COUNT(1)
        auto* literal = arena_.create<LiteralExpr>();
        literal->literal_type = LiteralType::INTEGER;
        literal->int_value = 1;
        expr->arguments.push_back(literal);

        advance();  // consume '*'
        parsed_count_star = true;
    }

    if (!parsed_count_star) {
        if (upper_name == "EXTRACT")
        {
            if (check(TokenType::RIGHT_PAREN))
            {
                error("EXTRACT requires arguments");
                return expr;
            }

            auto* extract_expr = arena_.create<ExtractExpr>();
            extract_expr->selector = parseElementSelector();
            auto emit_unknown_extract_field = [&](const std::string& field_name) {
                errorCode("PRS_0506", "EXTRACT_FIELD_UNKNOWN(" + toUpperAscii(field_name) + ")");
            };
            if (extract_expr->selector.kind == ElementSelector::Kind::IDENTIFIER) {
                std::string field_name;
                if (extract_expr->selector.identifier != StringPool::INVALID_ID) {
                    field_name = std::string(stringPool().get(extract_expr->selector.identifier));
                }
                if (!scratchbird::sblr::resolveExtractFieldName(field_name).has_value()) {
                    emit_unknown_extract_field(field_name);
                }
            } else if (extract_expr->selector.kind == ElementSelector::Kind::STRING_LITERAL) {
                std::string field_name;
                if (extract_expr->selector.string_literal != StringPool::INVALID_ID) {
                    field_name = std::string(stringPool().get(extract_expr->selector.string_literal));
                }
                if (!scratchbird::sblr::resolveExtractFieldName(field_name).has_value()) {
                    emit_unknown_extract_field(field_name);
                }
            }
            expect(TokenType::KW_FROM, "Expected FROM in EXTRACT expression");
            extract_expr->source = parseExpression();
            expect(TokenType::RIGHT_PAREN, "Expected ')' after EXTRACT expression");
            return extract_expr;
        }

        if (upper_name == "POSITION")
        {
            if (check(TokenType::RIGHT_PAREN))
            {
                error("POSITION requires arguments");
                return expr;
            }

            Expression* needle = parseAddExpr();
            expect(TokenType::KW_IN, "Expected IN in POSITION");
            Expression* haystack = parseAddExpr();
            expr->arguments.push_back(needle);
            expr->arguments.push_back(haystack);
            expect(TokenType::RIGHT_PAREN, "Expected ')' after POSITION arguments");
            return expr;
        }

        if (upper_name == "OVERLAY")
        {
            if (check(TokenType::RIGHT_PAREN))
            {
                error("OVERLAY requires arguments");
                return expr;
            }

            Expression* source = parseExpression();
            if (!expectContextual("PLACING", "Expected PLACING in OVERLAY"))
            {
                return expr;
            }
            Expression* replacement = parseExpression();
            expect(TokenType::KW_FROM, "Expected FROM in OVERLAY");
            Expression* start_pos = parseExpression();
            Expression* length = nullptr;
            if (matchContextual("FOR"))
            {
                length = parseExpression();
            }

            expr->arguments.push_back(source);
            expr->arguments.push_back(replacement);
            expr->arguments.push_back(start_pos);
            if (length)
            {
                expr->arguments.push_back(length);
            }

            expect(TokenType::RIGHT_PAREN, "Expected ')' after OVERLAY arguments");
            return expr;
        }

        // Parse arguments (allow ORDER BY within aggregate call).
        if (!check(TokenType::RIGHT_PAREN)) {
            if (matchContextual("DISTINCT")) {
                expr->distinct = true;
            } else if (matchContextual("ALL")) {
                expr->distinct = false;
            }

            if (!check(TokenType::RIGHT_PAREN)) {
                expr->arguments.push_back(parseExpression());
                while (match(TokenType::COMMA)) {
                    expr->arguments.push_back(parseExpression());
                }
            } else if (expr->distinct) {
                error("DISTINCT requires at least one argument");
            }

            if (match(TokenType::KW_ORDER) || matchContextual("ORDER")) {
                expectContextual("BY", "Expected BY after ORDER in aggregate");
                do {
                    OrderByItem* item = parseOrderByItem();
                    if (item) {
                        expr->order_by.push_back(item);
                    }
                } while (match(TokenType::COMMA));
            }
        }

        expect(TokenType::RIGHT_PAREN, "Expected ')' after function arguments");
    } else {
        if (match(TokenType::KW_ORDER) || matchContextual("ORDER")) {
            errorCode("PRS_0504", "COUNT(*) does not allow ORDER BY inside aggregate call");
            expectContextual("BY", "Expected BY after ORDER in aggregate");
            do {
                parseOrderByItem();
            } while (match(TokenType::COMMA));
        }
        if (match(TokenType::COMMA)) {
            errorCode("PRS_0504", "COUNT(*) cannot have additional arguments");
            while (!check(TokenType::RIGHT_PAREN) &&
                   !check(TokenType::END_OF_FILE)) {
                advance();
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after function arguments");
        }
        if (!check(TokenType::RIGHT_PAREN) &&
            !checkContextual("FILTER") &&
            !checkContextual("OVER")) {
            errorCode("PRS_0504", "COUNT(*) cannot have additional arguments");
            while (!check(TokenType::RIGHT_PAREN) &&
                   !check(TokenType::END_OF_FILE)) {
                advance();
            }
            if (check(TokenType::RIGHT_PAREN)) {
                advance();
            }
        } else if (check(TokenType::RIGHT_PAREN)) {
            advance();
        }
    }

    auto is_window_function = [&upper_name]() {
        return upper_name == "ROW_NUMBER" || upper_name == "RANK" ||
               upper_name == "DENSE_RANK" || upper_name == "LAG" ||
               upper_name == "LEAD" || upper_name == "FIRST_VALUE" ||
               upper_name == "LAST_VALUE" || upper_name == "NTH_VALUE";
    };

    if (is_window_function())
    {
        if (upper_name == "ROW_NUMBER" || upper_name == "RANK" || upper_name == "DENSE_RANK")
        {
            if (!expr->arguments.empty())
            {
                error(upper_name + " does not accept arguments");
            }
        }
        else if (upper_name == "LAG" || upper_name == "LEAD")
        {
            if (expr->arguments.empty() || expr->arguments.size() > 3)
            {
                error(upper_name + " requires 1 to 3 arguments");
            }
        }
        else if (upper_name == "FIRST_VALUE" || upper_name == "LAST_VALUE")
        {
            if (expr->arguments.size() != 1)
            {
                error(upper_name + " requires exactly one argument");
            }
        }
        else if (upper_name == "NTH_VALUE")
        {
            if (expr->arguments.size() != 2)
            {
                error("NTH_VALUE requires exactly two arguments");
            }
        }
    }

    if (matchContextual("FILTER"))
    {
        expect(TokenType::LEFT_PAREN, "Expected '(' after FILTER");
        expect(TokenType::KW_WHERE, "Expected WHERE in FILTER");
        expr->filter = parseExpression();
        expect(TokenType::RIGHT_PAREN, "Expected ')' after FILTER");
    }

    if (matchContextual("OVER"))
    {
        expr->is_window = true;
        expr->window = parseWindowSpec();
    }
    else if (is_window_function())
    {
        error(upper_name + " requires an OVER clause");
    }

    if (upper_name == "JSON_EXTRACT")
    {
        if (expr->arguments.size() < 2)
        {
            error("JSON_EXTRACT requires at least two arguments");
        }
    }
    else if (upper_name == "COALESCE")
    {
        if (expr->arguments.empty())
        {
            error("COALESCE requires at least one argument");
        }
    }
    else if (upper_name == "NULLIF")
    {
        if (expr->arguments.size() != 2)
        {
            error("NULLIF requires exactly two arguments");
        }
    }
    else if (upper_name == "JSON_OBJECT")
    {
        if ((expr->arguments.size() % 2) != 0)
        {
            error("JSON_OBJECT requires an even number of arguments");
        }
    }

    auto require_arity = [&](const char* name, size_t expected) {
        if (expr->arguments.size() != expected) {
            errorCode("PRS_0504",
                      std::string(name) + " requires exactly " + std::to_string(expected) + " argument(s)");
        }
    };

    auto require_feature_gate = [&](const char* feature_key) {
        requireFeature(feature_key);
    };

    if (upper_name == "DOC_PATH_EXISTS") {
        require_feature_gate(kFeatureDocPathFilter);
        require_arity("DOC_PATH_EXISTS", 2);
    } else if (upper_name == "DOC_PATH_GET_TEXT") {
        require_feature_gate(kFeatureDocPathFilter);
        require_arity("DOC_PATH_GET_TEXT", 2);
    } else if (upper_name == "DOC_PATH_GET_DOUBLE") {
        require_feature_gate(kFeatureDocPathFilter);
        require_arity("DOC_PATH_GET_DOUBLE", 2);
    } else if (upper_name == "DOC_PATH_GET_BOOL") {
        require_feature_gate(kFeatureDocPathFilter);
        require_arity("DOC_PATH_GET_BOOL", 2);
    } else if (upper_name == "TIME_BUCKET") {
        require_feature_gate(kFeatureTsBucketAgg);
        require_arity("TIME_BUCKET", 2);
    } else if (upper_name == "TS_INTERPOLATE_LINEAR") {
        require_feature_gate(kFeatureTsBucketAgg);
        require_arity("TS_INTERPOLATE_LINEAR", 5);
    } else if (upper_name == "TS_RATE_PER_SEC") {
        require_feature_gate(kFeatureTsBucketAgg);
        require_arity("TS_RATE_PER_SEC", 4);
    } else if (upper_name == "SEARCH_BM25") {
        require_feature_gate(kFeatureSearchQueryDsl);
        require_arity("SEARCH_BM25", 6);
    } else if (upper_name == "SEARCH_SCORE_FUSION") {
        require_feature_gate(kFeatureSearchQueryDsl);
        require_arity("SEARCH_SCORE_FUSION", 4);
    } else if (upper_name == "VECTOR_L2_DISTANCE") {
        require_feature_gate(kFeatureVectorAnn);
        require_arity("VECTOR_L2_DISTANCE", 2);
    } else if (upper_name == "VECTOR_COSINE_DISTANCE") {
        require_feature_gate(kFeatureVectorAnn);
        require_arity("VECTOR_COSINE_DISTANCE", 2);
    } else if (upper_name == "VECTOR_DOT_SIMILARITY") {
        require_feature_gate(kFeatureVectorAnn);
        require_arity("VECTOR_DOT_SIMILARITY", 2);
    } else if (upper_name == "VECTOR_L2_NORM") {
        require_feature_gate(kFeatureVectorAnn);
        require_arity("VECTOR_L2_NORM", 1);
    } else if (upper_name == "COMPILE_EMBEDDED_PAYLOAD") {
        require_feature_gate(kFeatureLanguageUdrCompileBridge);
        require_arity("COMPILE_EMBEDDED_PAYLOAD", 4);
    } else if (upper_name == "VALIDATE_EMBEDDED_PAYLOAD") {
        require_feature_gate(kFeatureLanguageUdrCompileBridge);
        require_arity("VALIDATE_EMBEDDED_PAYLOAD", 4);
    } else {
        auto starts_with = [&](const char* prefix) {
            const size_t n = std::strlen(prefix);
            return upper_name.size() >= n &&
                   std::equal(prefix, prefix + n, upper_name.begin());
        };

        if (starts_with("FN_GET")) {
            errorCode("PRS_0505",
                      "Legacy FN_GET* function names are not supported in v3; use EXTRACT(<selector> FROM <expr>)");
        } else if (starts_with("DOC_PATH_") ||
            starts_with("TS_") ||
            starts_with("SEARCH_") ||
            starts_with("VECTOR_")) {
            errorCode("PRS_0506", "Unknown builtin function symbol");
        }
    }

    return expr;
}

Expression* Parser::parseExtractExpr() {
    // Spec: docs/specifications/EXTRACT_AND_ALTER_ELEMENT.md
    auto* expr = arena_.create<ExtractExpr>();
    expect(TokenType::LEFT_PAREN, "Expected '(' after EXTRACT");
    expr->selector = parseElementSelector();
    auto emit_unknown_extract_field = [&](const std::string& field_name) {
        errorCode("PRS_0506", "EXTRACT_FIELD_UNKNOWN(" + toUpperAscii(field_name) + ")");
    };
    if (expr->selector.kind == ElementSelector::Kind::IDENTIFIER) {
        std::string field_name;
        if (expr->selector.identifier != StringPool::INVALID_ID) {
            field_name = std::string(stringPool().get(expr->selector.identifier));
        }
        if (!scratchbird::sblr::resolveExtractFieldName(field_name).has_value()) {
            emit_unknown_extract_field(field_name);
        }
    } else if (expr->selector.kind == ElementSelector::Kind::STRING_LITERAL) {
        std::string field_name;
        if (expr->selector.string_literal != StringPool::INVALID_ID) {
            field_name = std::string(stringPool().get(expr->selector.string_literal));
        }
        if (!scratchbird::sblr::resolveExtractFieldName(field_name).has_value()) {
            emit_unknown_extract_field(field_name);
        }
    }
    expect(TokenType::KW_FROM, "Expected FROM in EXTRACT expression");
    expr->source = parseExpression();
    expect(TokenType::RIGHT_PAREN, "Expected ')' after EXTRACT expression");
    return expr;
}

Expression* Parser::parseAlterElementExpr() {
    // Spec: docs/specifications/EXTRACT_AND_ALTER_ELEMENT.md
    auto* expr = arena_.create<AlterElementExpr>();
    expect(TokenType::LEFT_PAREN, "Expected '(' after ALTER_ELEMENT");
    expr->selector = parseElementSelector();
    expect(TokenType::KW_IN, "Expected IN in ALTER_ELEMENT expression");
    expr->source = parseExpression();
    expectContextual("TO", "Expected TO in ALTER_ELEMENT expression");
    expr->new_value = parseExpression();
    expect(TokenType::RIGHT_PAREN, "Expected ')' after ALTER_ELEMENT expression");
    return expr;
}

ElementSelector Parser::parseElementSelector() {
    ElementSelector selector;

    if (check(TokenType::STRING_LITERAL)) {
        selector.kind = ElementSelector::Kind::STRING_LITERAL;
        selector.string_literal = current().value.string_id;
        advance();
        return selector;
    }

    if (check(TokenType::INTEGER_LITERAL) || check(TokenType::PLUS) ||
        check(TokenType::MINUS) || check(TokenType::LEFT_PAREN)) {
        selector.kind = ElementSelector::Kind::INTEGER_EXPR;
        selector.expr = parseExpression();
        return selector;
    }

    if (isIdentifier()) {
        selector.kind = ElementSelector::Kind::IDENTIFIER;
        selector.identifier = expectIdentifier("Expected element identifier");
        if (match(TokenType::LEFT_PAREN)) {
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    selector.args.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after element selector arguments");
        }
        return selector;
    }

    error("Expected element selector");
    return selector;
}

Expression* Parser::parseParenExpr() {
    expect(TokenType::LEFT_PAREN, "Expected '('");
    Expression* expr = parseExpression();
    expect(TokenType::RIGHT_PAREN, "Expected ')'");
    return expr;
}

Expression* Parser::parseCastExpr() {
    auto* expr = arena_.create<CastExpr>();

    expect(TokenType::LEFT_PAREN, "Expected '(' after CAST");
    expr->expr = parseExpression();
    expect(TokenType::KW_AS, "Expected AS in CAST expression");
    expr->target_type = parseTypeName();
    // CAST ... USING <format> (see docs/specifications/DATA_TYPE_PERSISTENCE_AND_CASTS.md)
    if (match(TokenType::KW_USING)) {
        expr->format = expectIdentifier("Expected CAST USING format");
    }
    expect(TokenType::RIGHT_PAREN, "Expected ')' after CAST");

    return expr;
}

// =============================================================================
// Transaction Statements
// =============================================================================

StartTransactionStmt* Parser::parseStartTransaction() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<StartTransactionStmt>();

    // START TRANSACTION or BEGIN [TRANSACTION] [WORK]
    // We've already consumed BEGIN or START
    if (previous().type == TokenType::KW_START) {
        // START must be followed by TRANSACTION
        expectContextual("TRANSACTION", "Expected TRANSACTION after START");
    } else {
        // BEGIN can optionally have TRANSACTION or WORK
        matchContextual("TRANSACTION");
        matchContextual("WORK");
    }

    auto parseAutocommitMode = [&]() -> AutocommitMode {
        if (match(TokenType::KW_ON) || matchContextual("ON")) {
            return AutocommitMode::ON;
        }
        if (matchContextual("OFF")) {
            return AutocommitMode::OFF;
        }
        if (check(TokenType::INTEGER_LITERAL)) {
            int64_t value = current().value.int_value;
            advance();
            if (value == 0) {
                return AutocommitMode::OFF;
            }
            if (value == 1) {
                return AutocommitMode::ON;
            }
            error("AUTOCOMMIT expects 0/1 or ON/OFF");
            return AutocommitMode::UNCHANGED;
        }
        error("Expected AUTOCOMMIT mode (ON/OFF/1/0)");
        return AutocommitMode::UNCHANGED;
    };

    auto parseConflictClause =
        [&](TransactionConflictAction& action, bool& has_error_code, int32_t& error_code) {
            if (!matchContextual("CONFLICT")) {
                error("Expected CONFLICT after ON");
                return;
            }

            if (action != TransactionConflictAction::DEFAULT) {
                error("ON CONFLICT specified more than once");
            }

            if (match(TokenType::KW_COMMIT)) {
                action = TransactionConflictAction::COMMIT;
            } else if (match(TokenType::KW_ROLLBACK)) {
                action = TransactionConflictAction::ROLLBACK;
            } else if (matchContextual("ERROR")) {
                action = TransactionConflictAction::ERROR;
                if (check(TokenType::INTEGER_LITERAL)) {
                    int64_t value = current().value.int_value;
                    advance();
                    if (value < std::numeric_limits<int32_t>::min() ||
                        value > std::numeric_limits<int32_t>::max()) {
                        error("ON CONFLICT ERROR code out of range");
                    } else {
                        has_error_code = true;
                        error_code = static_cast<int32_t>(value);
                    }
                }
            } else if (matchContextual("KEEP")) {
                action = TransactionConflictAction::KEEP;
            } else {
                error("Expected conflict action (COMMIT, ROLLBACK, ERROR, KEEP)");
            }
        };

    auto applySnapshotIsolation = [&]() {
        stmt->has_isolation_level = true;
        if (matchContextual("TABLE")) {
            expectContextual("STABILITY", "Expected STABILITY after SNAPSHOT TABLE");
            stmt->isolation_level = IsolationLevel::SERIALIZABLE;
        } else {
            stmt->isolation_level = IsolationLevel::REPEATABLE_READ;
        }
    };

    auto parseReadCommittedVariant = [&]() {
        auto tokenMatches = [&](const Token& token, const char* keyword) -> bool {
            std::string_view text = state_.lexer().getTokenText(token);
            size_t len = std::strlen(keyword);
            if (text.size() != len) {
                return false;
            }
            for (size_t i = 0; i < len; ++i) {
                char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
                char b = static_cast<char>(std::tolower(static_cast<unsigned char>(keyword[i])));
                if (a != b) {
                    return false;
                }
            }
            return true;
        };

        if (stmt->has_read_committed_mode) {
            error("READ COMMITTED mode specified more than once");
            return;
        }
        if (matchContextual("READ")) {
            if (matchContextual("CONSISTENCY")) {
                stmt->has_read_committed_mode = true;
                stmt->read_committed_mode = ReadCommittedMode::READ_CONSISTENCY;
            } else {
                error("Expected CONSISTENCY after READ COMMITTED READ");
            }
        } else if (matchContextual("RECORD_VERSION")) {
            stmt->has_read_committed_mode = true;
            stmt->read_committed_mode = ReadCommittedMode::RECORD_VERSION;
        } else if (matchContextual("RECORD")) {
            expectContextual("VERSION", "Expected VERSION after RECORD");
            stmt->has_read_committed_mode = true;
            stmt->read_committed_mode = ReadCommittedMode::RECORD_VERSION;
        } else if (checkContextual("NO")) {
            Token next = state_.lexer().peekToken();
            if (next.type == TokenType::IDENTIFIER &&
                (tokenMatches(next, "RECORD") || tokenMatches(next, "RECORD_VERSION"))) {
                matchContextual("NO");
                if (matchContextual("RECORD_VERSION")) {
                    stmt->has_read_committed_mode = true;
                    stmt->read_committed_mode = ReadCommittedMode::NO_RECORD_VERSION;
                } else {
                    expectContextual("RECORD", "Expected RECORD after NO");
                    expectContextual("VERSION", "Expected VERSION after NO RECORD");
                    stmt->has_read_committed_mode = true;
                    stmt->read_committed_mode = ReadCommittedMode::NO_RECORD_VERSION;
                }
            }
        }
    };

    // Parse transaction characteristics (SQL-standard + Firebird legacy) in any order.
    while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
        if (match(TokenType::KW_ON)) {
            parseConflictClause(stmt->conflict_action, stmt->has_conflict_error_code,
                                stmt->conflict_error_code);
        } else if (matchContextual("ISOLATION")) {
            expectContextual("LEVEL", "Expected LEVEL after ISOLATION");

            stmt->has_isolation_level = true;
            if (matchContextual("READ")) {
                if (matchContextual("UNCOMMITTED")) {
                    stmt->isolation_level = IsolationLevel::READ_UNCOMMITTED;
                } else if (matchContextual("COMMITTED")) {
                    stmt->isolation_level = IsolationLevel::READ_COMMITTED;
                    parseReadCommittedVariant();
                } else {
                    error("Expected UNCOMMITTED or COMMITTED after READ");
                }
            } else if (matchContextual("REPEATABLE")) {
                expectContextual("READ", "Expected READ after REPEATABLE");
                stmt->isolation_level = IsolationLevel::REPEATABLE_READ;
            } else if (matchContextual("SERIALIZABLE")) {
                stmt->isolation_level = IsolationLevel::SERIALIZABLE;
            } else if (matchContextual("SNAPSHOT")) {
                applySnapshotIsolation();
            } else {
                error("Expected isolation level");
            }
        } else if (matchContextual("READ")) {
            if (matchContextual("ONLY")) {
                stmt->has_access_mode = true;
                stmt->access_mode = TransactionAccess::READ_ONLY;
            } else if (matchContextual("WRITE")) {
                stmt->has_access_mode = true;
                stmt->access_mode = TransactionAccess::READ_WRITE;
            } else if (matchContextual("COMMITTED")) {
                stmt->has_isolation_level = true;
                stmt->isolation_level = IsolationLevel::READ_COMMITTED;
                parseReadCommittedVariant();
            } else if (matchContextual("UNCOMMITTED")) {
                stmt->has_isolation_level = true;
                stmt->isolation_level = IsolationLevel::READ_UNCOMMITTED;
            } else {
                error("Expected ONLY, WRITE, COMMITTED, or UNCOMMITTED after READ");
            }
        } else if (matchContextual("SNAPSHOT")) {
            applySnapshotIsolation();
        } else if (matchContextual("DEFERRABLE")) {
            stmt->deferrable = true;
        } else if (match(TokenType::KW_NOT)) {
            if (matchContextual("DEFERRABLE")) {
                stmt->not_deferrable = true;
            } else if (matchContextual("WAIT")) {
                stmt->has_wait_mode = true;
                stmt->wait_mode = TransactionWaitMode::NO_WAIT;
            } else {
                error("Expected DEFERRABLE or WAIT after NOT");
            }
        } else if (matchContextual("WAIT")) {
            stmt->has_wait_mode = true;
            stmt->wait_mode = TransactionWaitMode::WAIT;
        } else if (matchContextual("NO")) {
            if (matchContextual("WAIT")) {
                stmt->has_wait_mode = true;
                stmt->wait_mode = TransactionWaitMode::NO_WAIT;
            } else {
                error("Expected WAIT after NO");
            }
        } else if (matchContextual("LOCK")) {
            expectContextual("TIMEOUT", "Expected TIMEOUT after LOCK");
            if (stmt->has_lock_timeout) {
                error("LOCK TIMEOUT specified more than once");
            }
            if (!check(TokenType::INTEGER_LITERAL)) {
                error("Expected integer literal after LOCK TIMEOUT");
            } else {
                int64_t value = current().value.int_value;
                advance();
                if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
                    error("LOCK TIMEOUT out of range");
                } else {
                    stmt->has_lock_timeout = true;
                    stmt->lock_timeout_seconds = static_cast<uint32_t>(value);
                }
            }
        } else if (matchContextual("RESERVING")) {
            // Table reservations are parsed into a simple list for later resolution.
            do {
                StringPool::StringId table_name =
                    expectIdentifier("Expected table name after RESERVING");
                expectContextual("FOR", "Expected FOR after RESERVING table name");

                TableLockMode lock_mode = TableLockMode::SHARED;
                if (matchContextual("SHARED")) {
                    lock_mode = TableLockMode::SHARED;
                } else if (matchContextual("PROTECTED")) {
                    lock_mode = TableLockMode::PROTECTED;
                } else {
                    error("Expected SHARED or PROTECTED in RESERVING clause");
                }

                bool for_write = false;
                if (matchContextual("READ")) {
                    for_write = false;
                } else if (matchContextual("WRITE")) {
                    for_write = true;
                } else {
                    error("Expected READ or WRITE in RESERVING clause");
                }

                stmt->table_reservations.emplace_back(table_name, lock_mode, for_write);
            } while (match(TokenType::COMMA));
        } else if (matchContextual("AUTOCOMMIT")) {
            stmt->has_autocommit = true;
            stmt->autocommit_mode = parseAutocommitMode();
        } else {
            // Unknown characteristic, stop parsing
            break;
        }

        // Optional comma between characteristics
        match(TokenType::COMMA);
    }

    stmt->span = makeSpan(start);
    return stmt;
}

PrepareTransactionStmt* Parser::parsePrepareTransaction() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<PrepareTransactionStmt>();

    // PREPARE TRANSACTION 'gid'
    expectContextual("TRANSACTION", "Expected TRANSACTION after PREPARE");

    if (!check(TokenType::STRING_LITERAL)) {
        error("Expected string literal after PREPARE TRANSACTION");
    } else {
        stmt->gid = current().value.string_id;
        advance();
    }

    stmt->span = makeSpan(start);
    return stmt;
}

CommitStmt* Parser::parseCommit() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<CommitStmt>();

    // COMMIT [WORK] [AND [NO] CHAIN]
    matchContextual("WORK");
    matchContextual("TRANSACTION");

    if (matchContextual("PREPARED")) {
        stmt->is_prepared = true;
        if (!check(TokenType::STRING_LITERAL)) {
            error("Expected string literal after COMMIT PREPARED");
        } else {
            stmt->prepared_gid = current().value.string_id;
            advance();
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (match(TokenType::KW_AND)) {
        if (matchContextual("NO")) {
            expectContextual("CHAIN", "Expected CHAIN after NO");
            stmt->and_no_chain = true;
        } else {
            expectContextual("CHAIN", "Expected CHAIN after AND");
            stmt->and_chain = true;
        }
    }

    if (matchContextual("RETAINING")) {
        stmt->retaining = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

RollbackStmt* Parser::parseRollback() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<RollbackStmt>();

    // ROLLBACK [WORK] [AND [NO] CHAIN]
    // ROLLBACK [WORK] TO [SAVEPOINT] name
    matchContextual("WORK");
    matchContextual("TRANSACTION");

    if (matchContextual("PREPARED")) {
        stmt->is_prepared = true;
        if (!check(TokenType::STRING_LITERAL)) {
            error("Expected string literal after ROLLBACK PREPARED");
        } else {
            stmt->prepared_gid = current().value.string_id;
            advance();
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("TO")) {
        matchContextual("SAVEPOINT");  // Optional SAVEPOINT keyword
        stmt->to_savepoint = true;
        stmt->savepoint_name = expectIdentifier("Expected savepoint name");
    } else if (match(TokenType::KW_AND)) {
        if (matchContextual("NO")) {
            expectContextual("CHAIN", "Expected CHAIN after NO");
            stmt->and_no_chain = true;
        } else {
            expectContextual("CHAIN", "Expected CHAIN after AND");
            stmt->and_chain = true;
        }
    }

    if (matchContextual("RETAINING")) {
        stmt->retaining = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

SavepointStmt* Parser::parseSavepoint() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<SavepointStmt>();

    // SAVEPOINT name
    stmt->name = expectIdentifier("Expected savepoint name");

    stmt->span = makeSpan(start);
    return stmt;
}

ReleaseSavepointStmt* Parser::parseReleaseSavepoint() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<ReleaseSavepointStmt>();

    // RELEASE [SAVEPOINT] name
    matchContextual("SAVEPOINT");  // Optional SAVEPOINT keyword
    stmt->name = expectIdentifier("Expected savepoint name");

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// Session Statements
// =============================================================================

SetStmt* Parser::parseSet() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<SetStmt>();

    // SET [SESSION | LOCAL] name = value
    // SET [SESSION | LOCAL] name TO value
    // SET [SESSION | LOCAL] name TO DEFAULT
    // SET TIME ZONE value
    // SET TRANSACTION ...
    // SET SESSION AUTHORIZATION ...
    // SET ROLE ...

    auto parseNameOrStringLiteral = [&](const char* error_message) -> StringPool::StringId {
        if (check(TokenType::STRING_LITERAL)) {
            auto id = current().value.string_id;
            advance();
            return id;
        }
        if (isIdentifier()) {
            return expectIdentifier(error_message);
        }
        error(error_message);
        return StringPool::INVALID_ID;
    };

    // Check for scope
    if (matchContextual("SESSION")) {
        stmt->scope = SetStmt::Scope::SESSION;
        // Could also be SET SESSION AUTHORIZATION
        if (matchContextual("AUTHORIZATION")) {
            stmt->set_type = SetStmt::SetType::SESSION_AUTHORIZATION;
            if (match(TokenType::KW_DEFAULT) || matchContextual("DEFAULT")) {
                stmt->is_default = true;
            } else {
                stmt->name = parseNameOrStringLiteral("Expected authorization user after SET SESSION AUTHORIZATION");
            }
            stmt->span = makeSpan(start);
            return stmt;
        }
    } else if (matchContextual("LOCAL")) {
        stmt->scope = SetStmt::Scope::LOCAL;
    }

    auto parseAutocommitMode = [&]() -> AutocommitMode {
        if (match(TokenType::KW_ON) || matchContextual("ON")) {
            return AutocommitMode::ON;
        }
        if (matchContextual("OFF")) {
            return AutocommitMode::OFF;
        }
        if (check(TokenType::INTEGER_LITERAL)) {
            int64_t value = current().value.int_value;
            advance();
            if (value == 0) {
                return AutocommitMode::OFF;
            }
            if (value == 1) {
                return AutocommitMode::ON;
            }
            error("AUTOCOMMIT expects 0/1 or ON/OFF");
            return AutocommitMode::UNCHANGED;
        }
        error("Expected AUTOCOMMIT mode (ON/OFF/1/0)");
        return AutocommitMode::UNCHANGED;
    };

    auto parseConflictClause =
        [&](TransactionConflictAction& action, bool& has_error_code, int32_t& error_code) {
            if (!matchContextual("CONFLICT")) {
                error("Expected CONFLICT after ON");
                return;
            }

            if (action != TransactionConflictAction::DEFAULT) {
                error("ON CONFLICT specified more than once");
            }

            if (match(TokenType::KW_COMMIT)) {
                action = TransactionConflictAction::COMMIT;
            } else if (match(TokenType::KW_ROLLBACK)) {
                action = TransactionConflictAction::ROLLBACK;
            } else if (matchContextual("ERROR")) {
                action = TransactionConflictAction::ERROR;
                if (check(TokenType::INTEGER_LITERAL)) {
                    int64_t value = current().value.int_value;
                    advance();
                    if (value < std::numeric_limits<int32_t>::min() ||
                        value > std::numeric_limits<int32_t>::max()) {
                        error("ON CONFLICT ERROR code out of range");
                    } else {
                        has_error_code = true;
                        error_code = static_cast<int32_t>(value);
                    }
                }
            } else if (matchContextual("KEEP")) {
                action = TransactionConflictAction::KEEP;
            } else {
                error("Expected conflict action (COMMIT, ROLLBACK, ERROR, KEEP)");
            }
        };

    auto applySnapshotIsolation = [&]() {
        stmt->has_isolation_level = true;
        if (matchContextual("TABLE")) {
            expectContextual("STABILITY", "Expected STABILITY after SNAPSHOT TABLE");
            stmt->isolation_level = IsolationLevel::SERIALIZABLE;
        } else {
            stmt->isolation_level = IsolationLevel::REPEATABLE_READ;
        }
    };

    auto parseReadCommittedVariant = [&]() {
        auto tokenMatches = [&](const Token& token, const char* keyword) -> bool {
            std::string_view text = state_.lexer().getTokenText(token);
            size_t len = std::strlen(keyword);
            if (text.size() != len) {
                return false;
            }
            for (size_t i = 0; i < len; ++i) {
                char a = static_cast<char>(std::tolower(static_cast<unsigned char>(text[i])));
                char b = static_cast<char>(std::tolower(static_cast<unsigned char>(keyword[i])));
                if (a != b) {
                    return false;
                }
            }
            return true;
        };

        if (stmt->has_read_committed_mode) {
            error("READ COMMITTED mode specified more than once");
            return;
        }
        if (matchContextual("READ")) {
            if (matchContextual("CONSISTENCY")) {
                stmt->has_read_committed_mode = true;
                stmt->read_committed_mode = ReadCommittedMode::READ_CONSISTENCY;
            } else {
                error("Expected CONSISTENCY after READ COMMITTED READ");
            }
        } else if (matchContextual("RECORD_VERSION")) {
            stmt->has_read_committed_mode = true;
            stmt->read_committed_mode = ReadCommittedMode::RECORD_VERSION;
        } else if (matchContextual("RECORD")) {
            expectContextual("VERSION", "Expected VERSION after RECORD");
            stmt->has_read_committed_mode = true;
            stmt->read_committed_mode = ReadCommittedMode::RECORD_VERSION;
        } else if (checkContextual("NO")) {
            Token next = state_.lexer().peekToken();
            if (next.type == TokenType::IDENTIFIER &&
                (tokenMatches(next, "RECORD") || tokenMatches(next, "RECORD_VERSION"))) {
                matchContextual("NO");
                if (matchContextual("RECORD_VERSION")) {
                    stmt->has_read_committed_mode = true;
                    stmt->read_committed_mode = ReadCommittedMode::NO_RECORD_VERSION;
                } else {
                    expectContextual("RECORD", "Expected RECORD after NO");
                    expectContextual("VERSION", "Expected VERSION after NO RECORD");
                    stmt->has_read_committed_mode = true;
                    stmt->read_committed_mode = ReadCommittedMode::NO_RECORD_VERSION;
                }
            }
        }
    };

    // Check for special SET variants
    if (matchContextual("TIME")) {
        expectContextual("ZONE", "Expected ZONE after TIME");
        stmt->set_type = SetStmt::SetType::TIME_ZONE;

        if (matchContextual("LOCAL") || matchContextual("DEFAULT")) {
            stmt->is_default = true;
        } else {
            stmt->value = parseExpression();
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("AUTOCOMMIT")) {
        stmt->set_type = SetStmt::SetType::AUTOCOMMIT;
        stmt->has_autocommit = true;
        stmt->autocommit_mode = parseAutocommitMode();

        if (match(TokenType::KW_ON)) {
            parseConflictClause(stmt->conflict_action, stmt->has_conflict_error_code,
                                stmt->conflict_error_code);
        }

        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("TRANSACTION")) {
        stmt->set_type = SetStmt::SetType::TRANSACTION;

        // Parse transaction characteristics (same as START TRANSACTION)
        while (!isAtEnd() && !check(TokenType::SEMICOLON)) {
            if (match(TokenType::KW_ON)) {
                parseConflictClause(stmt->conflict_action, stmt->has_conflict_error_code,
                                    stmt->conflict_error_code);
            } else if (matchContextual("ISOLATION")) {
                expectContextual("LEVEL", "Expected LEVEL after ISOLATION");

                stmt->has_isolation_level = true;
                if (matchContextual("READ")) {
                    if (matchContextual("UNCOMMITTED")) {
                        stmt->isolation_level = IsolationLevel::READ_UNCOMMITTED;
                    } else if (matchContextual("COMMITTED")) {
                        stmt->isolation_level = IsolationLevel::READ_COMMITTED;
                        parseReadCommittedVariant();
                    } else {
                        error("Expected UNCOMMITTED or COMMITTED after READ");
                    }
                } else if (matchContextual("REPEATABLE")) {
                    expectContextual("READ", "Expected READ after REPEATABLE");
                    stmt->isolation_level = IsolationLevel::REPEATABLE_READ;
                } else if (matchContextual("SERIALIZABLE")) {
                    stmt->isolation_level = IsolationLevel::SERIALIZABLE;
                } else if (matchContextual("SNAPSHOT")) {
                    applySnapshotIsolation();
                } else {
                    error("Expected isolation level");
                }
            } else if (matchContextual("READ")) {
                if (matchContextual("ONLY")) {
                    stmt->has_access_mode = true;
                    stmt->access_mode = TransactionAccess::READ_ONLY;
                } else if (matchContextual("WRITE")) {
                    stmt->has_access_mode = true;
                    stmt->access_mode = TransactionAccess::READ_WRITE;
                } else if (matchContextual("COMMITTED")) {
                    stmt->has_isolation_level = true;
                    stmt->isolation_level = IsolationLevel::READ_COMMITTED;
                    parseReadCommittedVariant();
                } else if (matchContextual("UNCOMMITTED")) {
                    stmt->has_isolation_level = true;
                    stmt->isolation_level = IsolationLevel::READ_UNCOMMITTED;
                } else {
                    error("Expected ONLY, WRITE, COMMITTED, or UNCOMMITTED after READ");
                }
            } else if (matchContextual("SNAPSHOT")) {
                applySnapshotIsolation();
            } else if (matchContextual("DEFERRABLE")) {
                stmt->deferrable = true;
            } else if (match(TokenType::KW_NOT)) {
                if (matchContextual("DEFERRABLE")) {
                    stmt->not_deferrable = true;
                } else if (matchContextual("WAIT")) {
                    stmt->has_wait_mode = true;
                    stmt->wait_mode = TransactionWaitMode::NO_WAIT;
                } else {
                    error("Expected DEFERRABLE or WAIT after NOT");
                }
            } else if (matchContextual("WAIT")) {
                stmt->has_wait_mode = true;
                stmt->wait_mode = TransactionWaitMode::WAIT;
            } else if (matchContextual("NO")) {
                if (matchContextual("WAIT")) {
                    stmt->has_wait_mode = true;
                    stmt->wait_mode = TransactionWaitMode::NO_WAIT;
                } else {
                    error("Expected WAIT after NO");
                }
            } else if (matchContextual("LOCK")) {
                expectContextual("TIMEOUT", "Expected TIMEOUT after LOCK");
                if (stmt->has_lock_timeout) {
                    error("LOCK TIMEOUT specified more than once");
                }
                if (!check(TokenType::INTEGER_LITERAL)) {
                    error("Expected integer literal after LOCK TIMEOUT");
                } else {
                    int64_t value = current().value.int_value;
                    advance();
                    if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
                        error("LOCK TIMEOUT out of range");
                    } else {
                        stmt->has_lock_timeout = true;
                        stmt->lock_timeout_seconds = static_cast<uint32_t>(value);
                    }
                }
            } else if (matchContextual("RESERVING")) {
                // Table reservations are parsed into a simple list for later resolution.
                do {
                    StringPool::StringId table_name =
                        expectIdentifier("Expected table name after RESERVING");
                    expectContextual("FOR", "Expected FOR after RESERVING table name");

                    TableLockMode lock_mode = TableLockMode::SHARED;
                    if (matchContextual("SHARED")) {
                        lock_mode = TableLockMode::SHARED;
                    } else if (matchContextual("PROTECTED")) {
                        lock_mode = TableLockMode::PROTECTED;
                    } else {
                        error("Expected SHARED or PROTECTED in RESERVING clause");
                    }

                    bool for_write = false;
                    if (matchContextual("READ")) {
                        for_write = false;
                    } else if (matchContextual("WRITE")) {
                        for_write = true;
                    } else {
                        error("Expected READ or WRITE in RESERVING clause");
                    }

                    stmt->table_reservations.emplace_back(table_name, lock_mode, for_write);
                } while (match(TokenType::COMMA));
            } else if (matchContextual("AUTOCOMMIT")) {
                stmt->has_autocommit = true;
                stmt->autocommit_mode = parseAutocommitMode();
            } else {
                break;
            }
            match(TokenType::COMMA);
        }

        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("CONSTRAINTS")) {
        stmt->set_type = SetStmt::SetType::CONSTRAINTS;
        stmt->name = stringPool().intern("CONSTRAINTS");

        if (matchContextual("ALL")) {
            stmt->constraints_all = true;
        } else {
            stmt->constraint_names.push_back(expectIdentifier("Expected constraint name or ALL"));
            while (match(TokenType::COMMA)) {
                stmt->constraint_names.push_back(expectIdentifier("Expected constraint name"));
            }
        }

        if (matchContextual("DEFERRED")) {
            stmt->constraints_deferred = true;
        } else if (matchContextual("IMMEDIATE")) {
            stmt->constraints_deferred = false;
        } else {
            errorCode("PRS_0504", "Expected DEFERRED or IMMEDIATE in SET CONSTRAINTS");
        }

        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("SQL")) {
        expectContextual("DIALECT", "Expected DIALECT after SQL");
        stmt->set_type = SetStmt::SetType::SQL_DIALECT;

        if (check(TokenType::INTEGER_LITERAL)) {
            int64_t dialect = current().value.int_value;
            advance();
            if (dialect >= 1 && dialect <= 3) {
                stmt->sql_dialect = static_cast<uint8_t>(dialect);
            } else {
                error("SQL DIALECT must be 1, 2, or 3");
            }
        } else {
            error("Expected SQL dialect number (1, 2, or 3)");
        }

        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("NAMES")) {
        stmt->set_type = SetStmt::SetType::NAMES;
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected character set name after SET NAMES");
        } else {
            error("Expected character set name after SET NAMES");
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("LOCAL_TIMEOUT")) {
        stmt->set_type = SetStmt::SetType::LOCAL_TIMEOUT;
        if (check(TokenType::INTEGER_LITERAL)) {
            int64_t value = current().value.int_value;
            advance();
            if (value < 0 || value > std::numeric_limits<uint32_t>::max()) {
                error("LOCAL_TIMEOUT out of range");
            } else {
                stmt->local_timeout_seconds = static_cast<uint32_t>(value);
            }
        } else {
            error("Expected integer literal after SET LOCAL_TIMEOUT");
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    auto makeStringLiteral = [&](const std::string& value_text) -> Expression* {
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(value_text);
        return lit;
    };

    if (matchContextual("CONSISTENCY")) {
        stmt->set_type = SetStmt::SetType::VARIABLE;
        stmt->name = stringPool().intern("runtime.consistency.default");
        if (!isIdentifier()) {
            errorCode("PRS_0504", "Expected consistency level after SET CONSISTENCY");
        } else {
            std::string level = std::string(stringPool().get(expectIdentifier(
                "Expected consistency level after SET CONSISTENCY")));
            for (char& c : level) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            stmt->value = makeStringLiteral(level);
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("SERIAL")) {
        expectContextual("CONSISTENCY", "Expected CONSISTENCY after SET SERIAL");
        stmt->set_type = SetStmt::SetType::VARIABLE;
        stmt->name = stringPool().intern("runtime.consistency.serial_default");
        if (!isIdentifier()) {
            errorCode("PRS_0504", "Expected serial consistency level after SET SERIAL CONSISTENCY");
        } else {
            std::string level = std::string(stringPool().get(expectIdentifier(
                "Expected serial consistency level after SET SERIAL CONSISTENCY")));
            for (char& c : level) {
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }
            stmt->value = makeStringLiteral(level);
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("CONCURRENCY")) {
        expectContextual("MODE", "Expected MODE after SET CONCURRENCY");
        stmt->set_type = SetStmt::SetType::VARIABLE;
        stmt->name = stringPool().intern("runtime.concurrency.mode");
        std::string mode;
        if (matchContextual("SINGLE_WRITER")) {
            mode = "SINGLE_WRITER";
        } else if (matchContextual("MULTI_WRITER")) {
            mode = "MULTI_WRITER";
        } else if (matchContextual("AUTO")) {
            mode = "AUTO";
        } else {
            errorCode("PRS_0504", "Expected SINGLE_WRITER, MULTI_WRITER, or AUTO after SET CONCURRENCY MODE");
        }
        if (!mode.empty()) {
            stmt->value = makeStringLiteral(mode);
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("SINGLE_WRITER")) {
        stmt->set_type = SetStmt::SetType::VARIABLE;
        stmt->name = stringPool().intern("runtime.concurrency.mode");
        std::string mode;
        if (match(TokenType::KW_ON) || matchContextual("ON")) {
            mode = "SINGLE_WRITER";
        } else if (matchContextual("OFF")) {
            mode = "MULTI_WRITER";
        } else {
            errorCode("PRS_0504", "Expected ON or OFF after SET SINGLE_WRITER");
        }
        if (!mode.empty()) {
            stmt->value = makeStringLiteral(mode);
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("SEQUENCE") || matchContextual("GENERATOR")) {
        stmt->set_type = SetStmt::SetType::GENERATOR;
        stmt->name = expectIdentifier("Expected sequence name after SET SEQUENCE");
        if (!matchContextual("TO") && !match(TokenType::EQUAL)) {
            error("Expected TO or '=' in SET SEQUENCE");
        } else {
            stmt->value = parseExpression();
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("ROLE")) {
        stmt->set_type = SetStmt::SetType::ROLE;
        if (matchContextual("NONE") || match(TokenType::KW_DEFAULT) || matchContextual("DEFAULT")) {
            stmt->is_default = true;
        } else {
            stmt->name = parseNameOrStringLiteral("Expected role name after SET ROLE");
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("TERM")) {
        stmt->set_type = SetStmt::SetType::TERM;
        stmt->name = stringPool().intern("TERM");

        auto parseTermToken = [&](bool allow_semicolon) -> std::string {
            if (isAtEnd()) {
                return {};
            }

            if (check(TokenType::SEMICOLON)) {
                if (!allow_semicolon) {
                    return {};
                }
                advance();
                return ";";
            }

            if (check(TokenType::STRING_LITERAL) || isIdentifier()) {
                std::string token = std::string(state_.getTokenText(current()));
                advance();
                return token;
            }

            TokenType punctuation = current().type;
            if (punctuation == TokenType::END_OF_FILE) {
                return {};
            }

            std::string token;
            if (punctuation == TokenType::CARET) {
                while (check(TokenType::CARET)) {
                    token.append(state_.getTokenText(current()));
                    advance();
                }
                return token;
            }

            token = std::string(state_.getTokenText(current()));
            advance();
            return token;
        };

        std::string new_terminator = parseTermToken(true);
        if (new_terminator.empty()) {
            errorCode("PRS_0504", "SET TERM requires new terminator token");
        }

        std::string old_terminator;
        if (!isAtEnd() && !check(TokenType::SEMICOLON)) {
            old_terminator = parseTermToken(true);
        }

        if (!isAtEnd() && !check(TokenType::SEMICOLON)) {
            errorCode("PRS_0504", "SET TERM supports only new and optional old terminator tokens");
        }

        std::string payload = new_terminator;
        if (!old_terminator.empty()) {
            payload.push_back(' ');
            payload.append(old_terminator);
        }
        stmt->value = makeStringLiteral(payload);

        stmt->span = makeSpan(start);
        return stmt;
    }

    auto parseSchemaSettingValue = [&]() -> Expression* {
        if (check(TokenType::STRING_LITERAL)) {
            auto* lit = arena_.create<LiteralExpr>();
            lit->literal_type = LiteralType::STRING;
            lit->string_value = current().value.string_id;
            advance();
            return lit;
        }

        if (isIdentifier() || check(TokenType::DOT) || check(TokenType::EXCLAIM_COLON)) {
            SchemaPath path = parseSchemaPath(state_);
            return makeStringLiteral(schemaPathToString(path, stringPool()));
        }

        errorCode("PRS_0504", "Expected schema path or DEFAULT");
        return makeStringLiteral("");
    };

    if (matchContextual("SCHEMA") || matchContextual("CURRENT_SCHEMA")) {
        stmt->set_type = SetStmt::SetType::VARIABLE;
        stmt->name = stringPool().intern("CURRENT_SCHEMA");

        // Accept both SQL-standard form (SET SCHEMA name) and assignment form.
        match(TokenType::EQUAL);
        matchContextual("TO");

        if (match(TokenType::KW_DEFAULT) || matchContextual("DEFAULT")) {
            stmt->is_default = true;
        } else {
            stmt->value = parseSchemaSettingValue();
        }

        stmt->span = makeSpan(start);
        return stmt;
    }

    auto parseVariableAssignment = [&](StringPool::StringId name_id) -> SetStmt* {
        stmt->set_type = SetStmt::SetType::VARIABLE;
        stmt->name = name_id;

        auto makeBooleanLiteral = [&](bool value) -> Expression* {
            auto* lit = arena_.create<LiteralExpr>();
            lit->literal_type = LiteralType::BOOLEAN;
            lit->bool_value = value;
            return lit;
        };

        // = or TO
        const bool has_assignment_operator =
            match(TokenType::EQUAL) || matchContextual("TO");
        if (!has_assignment_operator) {
            // Support shorthand assignments (for example: SET operator.strict_mode ON).
            if (!check(TokenType::KW_DEFAULT) && !checkContextual("DEFAULT") &&
                !check(TokenType::KW_ON) && !checkContextual("ON") &&
                !checkContextual("OFF") &&
                !check(TokenType::KW_TRUE) && !check(TokenType::KW_FALSE) &&
                !check(TokenType::INTEGER_LITERAL) && !check(TokenType::FLOAT_LITERAL) &&
                !check(TokenType::STRING_LITERAL) && !check(TokenType::BLOB_LITERAL) &&
                !check(TokenType::LEFT_PAREN) && !isIdentifier()) {
                error("Expected '=' or TO after variable name");
            }
        }

        // Value can be DEFAULT or an expression (or list of values)
        if (match(TokenType::KW_DEFAULT) || matchContextual("DEFAULT")) {
            stmt->is_default = true;
        } else if (match(TokenType::KW_ON) || matchContextual("ON")) {
            stmt->value = makeBooleanLiteral(true);
        } else if (matchContextual("OFF")) {
            stmt->value = makeBooleanLiteral(false);
        } else {
            // Parse value(s) - some settings accept comma-separated lists
            stmt->value = parseExpression();
            while (match(TokenType::COMMA)) {
                stmt->values.push_back(stmt->value);
                stmt->value = parseExpression();
            }
            if (!stmt->values.empty()) {
                stmt->values.push_back(stmt->value);
                stmt->value = nullptr;  // Use values list instead
            }
        }

        stmt->span = makeSpan(start);
        return stmt;
    };

    auto parseVariableName = [&]() -> StringPool::StringId {
        StringPool::StringId first =
            expectIdentifier("Expected variable name");
        if (first == StringPool::INVALID_ID) {
            return first;
        }
        std::string name = std::string(stringPool().get(first));
        while (match(TokenType::DOT)) {
            StringPool::StringId next =
                expectIdentifier("Expected identifier after '.' in variable name");
            if (next == StringPool::INVALID_ID) {
                return StringPool::INVALID_ID;
            }
            name.push_back('.');
            name.append(stringPool().get(next));
        }
        return stringPool().intern(name);
    };

    if (matchContextual("PARSER")) {
        if (matchContextual("VERSION")) {
            if (check(TokenType::INTEGER_LITERAL)) {
                advance();
            }
            error("SET PARSER VERSION is not supported");
            stmt->span = makeSpan(start);
            return stmt;
        }
        return parseVariableAssignment(stringPool().intern("PARSER"));
    }

    // Regular SET name = value / SET name TO value
    StringPool::StringId var_name = parseVariableName();
    return parseVariableAssignment(var_name);
}

ResetStmt* Parser::parseReset() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<ResetStmt>();

    auto parseVariableName = [&]() -> StringPool::StringId {
        StringPool::StringId first =
            expectIdentifier("Expected variable name or ALL");
        if (first == StringPool::INVALID_ID) {
            return first;
        }
        std::string name = std::string(stringPool().get(first));
        while (match(TokenType::DOT)) {
            StringPool::StringId next =
                expectIdentifier("Expected identifier after '.' in variable name");
            if (next == StringPool::INVALID_ID) {
                return StringPool::INVALID_ID;
            }
            name.push_back('.');
            name.append(stringPool().get(next));
        }
        return stringPool().intern(name);
    };

    // RESET name
    // RESET ALL
    if (matchContextual("ALL")) {
        stmt->reset_all = true;
    } else if (matchContextual("SESSION")) {
        expectContextual("AUTHORIZATION", "Expected AUTHORIZATION after RESET SESSION");
        stmt->name = stringPool().intern("SESSION_AUTHORIZATION");
    } else if (matchContextual("ROLE")) {
        stmt->name = stringPool().intern("ROLE");
    } else if (matchContextual("TIME")) {
        expectContextual("ZONE", "Expected ZONE after RESET TIME");
        stmt->name = stringPool().intern("TIME_ZONE");
    } else {
        stmt->name = parseVariableName();
    }

    stmt->span = makeSpan(start);
    return stmt;
}

ShowStmt* Parser::parseShow() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<ShowStmt>();

    auto parsePathString = [&](const char* message) -> StringPool::StringId {
        if (!canStartSchemaPath(state_)) {
            error(message);
            return StringPool::INVALID_ID;
        }
        SchemaPath path = parseSchemaPath(state_);
        return stringPool().intern(schemaPathToString(path, stringPool()));
    };

    auto parseLikeValue = [&]() {
        if (check(TokenType::STRING_LITERAL) || check(TokenType::IDENTIFIER)) {
            stmt->like_pattern = current().value.string_id;
            advance();
            return;
        }
        error("Expected pattern after LIKE");
    };

    auto parseLikeClause = [&]() {
        if (match(TokenType::KW_LIKE)) {
            parseLikeValue();
        }
    };

    // Optional scope clause for canonical surface. Both IN and FROM are accepted.
    auto parseScopeClause = [&]() {
        if (match(TokenType::KW_IN) || match(TokenType::KW_FROM)) {
            stmt->from_name = parsePathString("Expected schema/object path after IN/FROM");
        }
    };

    auto parseRecursiveClause = [&]() {
        if (!match(TokenType::KW_WITH)) {
            return;
        }
        expectContextual("RECURSIVE", "Expected RECURSIVE after WITH");
        stmt->recursive = true;
        stmt->max_depth = 1;  // Contract default when WITH RECURSIVE has no explicit depth.
        if (matchContextual("MAX")) {
            expectContextual("DEPTH", "Expected DEPTH after MAX");
            if (!check(TokenType::INTEGER_LITERAL)) {
                error("Expected integer after MAX DEPTH");
                return;
            }
            int64_t depth = current().value.int_value;
            advance();
            if (depth < 1) {
                error("WITH RECURSIVE MAX DEPTH requires n >= 1");
                return;
            }
            stmt->max_depth = static_cast<uint32_t>(depth);
        }
    };

    auto parseVariableName = [&](const char* error_message) -> StringPool::StringId {
        StringPool::StringId first = expectIdentifier(error_message);
        if (first == StringPool::INVALID_ID) {
            return first;
        }
        std::string name = std::string(stringPool().get(first));
        while (match(TokenType::DOT)) {
            StringPool::StringId next =
                expectIdentifier("Expected identifier after '.' in variable name");
            if (next == StringPool::INVALID_ID) {
                return StringPool::INVALID_ID;
            }
            name.push_back('.');
            name.append(stringPool().get(next));
        }
        return stringPool().intern(name);
    };

    auto setUnifiedType = [&](const char* object_type) {
        stmt->unified_metadata = true;
        stmt->metadata_object_type = stringPool().intern(object_type);
    };

    auto matchUnifiedObjectType = [&](std::string& object_type) -> bool {
        struct Entry {
            const char* singular;
            const char* plural;
            const char* canonical;
        };
        static const Entry kEntries[] = {
            {"SCHEMA", "SCHEMAS", "SCHEMA"},
            {"TABLE", "TABLES", "TABLE"},
            {"VIEW", "VIEWS", "VIEW"},
            {"COLUMN", "COLUMNS", "COLUMN"},
            {"INDEX", "INDEXES", "INDEX"},
            {"SEQUENCE", "SEQUENCES", "SEQUENCE"},
            {"GENERATOR", "GENERATORS", "SEQUENCE"},
            {"DOMAIN", "DOMAINS", "DOMAIN"},
            {"FUNCTION", "FUNCTIONS", "FUNCTION"},
            {"PROCEDURE", "PROCEDURES", "PROCEDURE"},
            {"TRIGGER", "TRIGGERS", "TRIGGER"},
            {"PACKAGE", "PACKAGES", "PACKAGE"},
            {"ROLE", "ROLES", "ROLE"},
            {"DATABASE", "DATABASES", "DATABASE"},
            {"USER", "USERS", "USER"},
            {"GROUP", "GROUPS", "GROUP"},
            {"TYPE", "TYPES", "TYPE"},
            {"SERVER", "SERVERS", "SERVER"},
            {"TABLESPACE", "TABLESPACES", "TABLESPACE"},
            {"POLICY", "POLICIES", "POLICY"},
            {"JOB", "JOBS", "JOB"},
        };

        for (const auto& entry : kEntries) {
            if (matchContextual(entry.singular) || matchContextual(entry.plural)) {
                object_type = entry.canonical;
                return true;
            }
        }
        return false;
    };

    auto matchSchemaPathCurrentSchemaVariable = [&]() -> bool {
        auto next_is = [&](const char* word) -> bool {
            Token lookahead = state_.lexer().peekToken();
            return lookahead.type == TokenType::IDENTIFIER &&
                   caseInsensitiveEquals(state_.lexer().getTokenText(lookahead.span), word);
        };

        if (matchContextual("CURRENT_SCHEMA")) {
            stmt->show_type = ShowStmt::ShowType::VARIABLE;
            stmt->name = stringPool().intern("CURRENT_SCHEMA");
            return true;
        }
        if (matchContextual("SEARCH_PATH") || matchContextual("SCHEMA_PATH")) {
            stmt->show_type = ShowStmt::ShowType::VARIABLE;
            stmt->name = stringPool().intern("SEARCH_PATH");
            return true;
        }
        if (checkContextual("CURRENT") && next_is("SCHEMA")) {
            matchContextual("CURRENT");
            matchContextual("SCHEMA");
            stmt->show_type = ShowStmt::ShowType::VARIABLE;
            stmt->name = stringPool().intern("CURRENT_SCHEMA");
            return true;
        }
        if (checkContextual("SEARCH") && next_is("PATH")) {
            matchContextual("SEARCH");
            matchContextual("PATH");
            stmt->show_type = ShowStmt::ShowType::VARIABLE;
            stmt->name = stringPool().intern("SEARCH_PATH");
            return true;
        }
        if (checkContextual("SCHEMA") && next_is("PATH")) {
            matchContextual("SCHEMA");
            matchContextual("PATH");
            stmt->show_type = ShowStmt::ShowType::VARIABLE;
            stmt->name = stringPool().intern("SEARCH_PATH");
            return true;
        }
        return false;
    };

    // Canonical prefix form:
    // SHOW [IN|FROM <path>] <object_type> ...
    if (check(TokenType::KW_IN) || check(TokenType::KW_FROM)) {
        parseScopeClause();
        std::string object_type;
        if (matchContextual("ALL")) {
            object_type = "ALL";
        } else if (!matchUnifiedObjectType(object_type)) {
            error("Expected object type after SHOW IN/FROM <path>");
            stmt->span = makeSpan(start);
            return stmt;
        }

        stmt->show_type = ShowStmt::ShowType::OBJECTS;
        setUnifiedType(object_type.c_str());

        if (match(TokenType::KW_LIKE)) {
            parseLikeValue();
        } else if (check(TokenType::IDENTIFIER) || check(TokenType::STRING_LITERAL)) {
            stmt->name = current().value.string_id;
            advance();
        }
        parseRecursiveClause();

        stmt->span = makeSpan(start);
        return stmt;
    }

    // SHOW ALL
    if (matchContextual("ALL")) {
        // Canonical variant:
        // SHOW ALL [IN|FROM <path>] [<name> | LIKE <pattern>] [WITH RECURSIVE ...]
        if (check(TokenType::KW_IN) || check(TokenType::KW_FROM) || check(TokenType::KW_LIKE) ||
            check(TokenType::IDENTIFIER) || check(TokenType::STRING_LITERAL) ||
            check(TokenType::KW_WITH)) {
            stmt->show_type = ShowStmt::ShowType::OBJECTS;
            setUnifiedType("ALL");
            parseScopeClause();
            if (match(TokenType::KW_LIKE)) {
                parseLikeValue();
            } else if (check(TokenType::IDENTIFIER) || check(TokenType::STRING_LITERAL)) {
                stmt->name = current().value.string_id;
                advance();
            }
            parseRecursiveClause();
        } else {
            stmt->show_type = ShowStmt::ShowType::ALL;
            setUnifiedType("ALL");
        }
    }
    // SHOW TRANSACTION ISOLATION LEVEL
    else if (matchContextual("TRANSACTION")) {
        expectContextual("ISOLATION", "Expected ISOLATION after TRANSACTION");
        expectContextual("LEVEL", "Expected LEVEL after ISOLATION");
        stmt->show_type = ShowStmt::ShowType::TRANSACTION_ISOLATION_LEVEL;
    }
    // SHOW CURRENT SCHEMA / SHOW CURRENT_SCHEMA / SHOW SEARCH PATH / SHOW SEARCH_PATH / SHOW SCHEMA PATH
    else if (matchSchemaPathCurrentSchemaVariable()) {
        // Handled by helper.
    }
    // SHOW TABLES [FROM db] [LIKE pattern]
    else if (matchContextual("TABLES")) {
        stmt->show_type = ShowStmt::ShowType::TABLES;
        setUnifiedType("TABLE");
        parseScopeClause();
        parseLikeClause();
        parseRecursiveClause();
    }
    // SHOW DATABASES [LIKE pattern]
    else if (matchContextual("DATABASES")) {
        stmt->show_type = ShowStmt::ShowType::DATABASES;
        setUnifiedType("DATABASE");
        parseScopeClause();
        parseLikeClause();
        parseRecursiveClause();
    }
    // SHOW COLUMNS FROM table [LIKE pattern]
    else if (matchContextual("COLUMNS")) {
        stmt->show_type = ShowStmt::ShowType::COLUMNS;
        setUnifiedType("COLUMN");
        if (check(TokenType::KW_IN) || check(TokenType::KW_FROM)) {
            parseScopeClause();
        } else {
            expect(TokenType::KW_FROM, "Expected FROM/IN after COLUMNS");
            stmt->from_name = parsePathString("Expected table path after FROM");
        }
        parseLikeClause();
        parseRecursiveClause();
    }
    // SHOW INDEX... family
    else if (matchContextual("INDEXES")) {
        stmt->show_type = ShowStmt::ShowType::INDEXES;
        setUnifiedType("INDEX");
        parseScopeClause();
        parseRecursiveClause();
    } else if (matchContextual("INDEX")) {
        setUnifiedType("INDEX");
        if (matchContextual("HEALTH")) {
            stmt->show_type = ShowStmt::ShowType::INDEX_HEALTH;
            stmt->name = parsePathString("Expected index path after SHOW INDEX HEALTH");
        } else if (matchContextual("USAGE")) {
            stmt->show_type = ShowStmt::ShowType::INDEX_USAGE;
            stmt->name = parsePathString("Expected index path after SHOW INDEX USAGE");
        } else if (matchContextual("STORAGE")) {
            stmt->show_type = ShowStmt::ShowType::INDEX_STORAGE;
            stmt->name = parsePathString("Expected index path after SHOW INDEX STORAGE");
        } else if (matchContextual("CONTENTION")) {
            stmt->show_type = ShowStmt::ShowType::INDEX_CONTENTION;
            stmt->name = parsePathString("Expected index path after SHOW INDEX CONTENTION");
        } else if (matchContextual("OPTIONS")) {
            stmt->show_type = ShowStmt::ShowType::INDEX_OPTIONS;
            stmt->name = parsePathString("Expected index path after SHOW INDEX OPTIONS");
        } else if (check(TokenType::KW_FROM) || check(TokenType::KW_IN)) {
            stmt->show_type = ShowStmt::ShowType::INDEXES;
            parseScopeClause();
        } else if (check(TokenType::IDENTIFIER) || check(TokenType::STRING_LITERAL)) {
            // SHOW INDEX name - Firebird style
            stmt->show_type = ShowStmt::ShowType::INDEX;
            stmt->name = parsePathString("Expected index path");
        } else {
            // SHOW INDEX (list all)
            stmt->show_type = ShowStmt::ShowType::INDEXES;
        }
        parseRecursiveClause();
    }
    // SHOW CREATE TABLE name
    else if (match(TokenType::KW_CREATE) || matchContextual("CREATE")) {
        expectContextual("TABLE", "Expected TABLE after CREATE");
        stmt->show_type = ShowStmt::ShowType::CREATE_TABLE;
        stmt->name = expectIdentifier("Expected table name");
    }
    // SHOW TABLE name - Firebird style detailed table info
    else if (matchContextual("TABLE")) {
        setUnifiedType("TABLE");
        parseScopeClause();
        if (check(TokenType::IDENTIFIER)) {
            stmt->show_type = ShowStmt::ShowType::TABLE;
            stmt->name = expectIdentifier("Expected table name");
        } else {
            stmt->show_type = ShowStmt::ShowType::TABLES;
        }
        parseRecursiveClause();
    }
    // SHOW TRIGGER name
    else if (matchContextual("TRIGGER") || matchContextual("TRIGGERS")) {
        stmt->show_type = ShowStmt::ShowType::TRIGGER;
        setUnifiedType("TRIGGER");
        parseScopeClause();
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected trigger name");
        }
        parseRecursiveClause();
    }
    // SHOW VIEW name
    else if (matchContextual("VIEW") || matchContextual("VIEWS")) {
        stmt->show_type = ShowStmt::ShowType::VIEW;
        setUnifiedType("VIEW");
        parseScopeClause();
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected view name");
        }
        parseRecursiveClause();
    }
    // SHOW PROCEDURE name
    else if (matchContextual("PROCEDURE") || matchContextual("PROCEDURES")) {
        stmt->show_type = ShowStmt::ShowType::PROCEDURE;
        setUnifiedType("PROCEDURE");
        parseScopeClause();
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected procedure name");
        }
        parseRecursiveClause();
    }
    // SHOW FUNCTION name
    else if (matchContextual("FUNCTION") || matchContextual("FUNCTIONS")) {
        stmt->show_type = ShowStmt::ShowType::FUNCTION;
        setUnifiedType("FUNCTION");
        parseScopeClause();
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected function name");
        }
        parseRecursiveClause();
    }
    // SHOW DOMAIN name
    else if (matchContextual("DOMAIN") || matchContextual("DOMAINS")) {
        stmt->show_type = ShowStmt::ShowType::DOMAIN;
        setUnifiedType("DOMAIN");
        parseScopeClause();
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected domain name");
        }
        parseRecursiveClause();
    }
    // SHOW GENERATOR/SEQUENCE name
    else if (matchContextual("GENERATOR") || matchContextual("GENERATORS") ||
             matchContextual("SEQUENCE") || matchContextual("SEQUENCES")) {
        stmt->show_type = ShowStmt::ShowType::GENERATOR;
        setUnifiedType("SEQUENCE");
        parseScopeClause();
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected sequence/generator name");
        }
        parseRecursiveClause();
    }
    // SHOW SCHEMA [name]
    else if (matchContextual("SCHEMA") || matchContextual("SCHEMAS")) {
        stmt->show_type = ShowStmt::ShowType::SCHEMA;
        setUnifiedType("SCHEMA");
        parseScopeClause();
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected schema name");
        }
        parseRecursiveClause();
    }
    // SHOW ROLE name
    else if (matchContextual("ROLE") || matchContextual("ROLES")) {
        stmt->show_type = ShowStmt::ShowType::ROLE;
        setUnifiedType("ROLE");
        parseScopeClause();
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected role name");
        }
        parseRecursiveClause();
    }
    // SHOW GRANTS [FOR name]
    else if (matchContextual("GRANTS")) {
        stmt->show_type = ShowStmt::ShowType::GRANTS;
        if (matchContextual("FOR")) {
            stmt->name = expectIdentifier("Expected object name after FOR");
        }
    }
    // SHOW JOBS [LIKE pattern]
    else if (matchContextual("JOBS")) {
        stmt->show_type = ShowStmt::ShowType::JOBS;
        setUnifiedType("JOB");
        parseScopeClause();
        parseLikeClause();
        parseRecursiveClause();
    }
    // SHOW JOB name or SHOW JOB RUNS [FOR] job_name
    else if (matchContextual("JOB")) {
        setUnifiedType("JOB");
        parseScopeClause();
        if (matchContextual("RUNS")) {
            stmt->show_type = ShowStmt::ShowType::JOB_RUNS;
            if (matchContextual("FOR")) {
                // optional
            }
            if (check(TokenType::STRING_LITERAL)) {
                stmt->name = current().value.string_id;
                advance();
            } else {
                stmt->name = expectIdentifier("Expected job name");
            }
        } else {
            stmt->show_type = ShowStmt::ShowType::JOB;
            if (check(TokenType::STRING_LITERAL)) {
                stmt->name = current().value.string_id;
                advance();
            } else {
                stmt->name = expectIdentifier("Expected job name");
            }
        }
        parseRecursiveClause();
    }
    // SHOW CHECKS table
    else if (matchContextual("CHECKS") || matchContextual("CHECK")) {
        stmt->show_type = ShowStmt::ShowType::CHECKS;
        parseScopeClause();
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected table name");
        }
        parseRecursiveClause();
    }
    // SHOW COLLATIONS [LIKE pattern]
    else if (matchContextual("COLLATIONS") || matchContextual("COLLATION")) {
        stmt->show_type = ShowStmt::ShowType::COLLATIONS;
        parseScopeClause();
        parseLikeClause();
        parseRecursiveClause();
    }
    // SHOW COMMENTS [object_name]
    else if (matchContextual("COMMENTS") || matchContextual("COMMENT")) {
        stmt->show_type = ShowStmt::ShowType::COMMENTS;
        parseScopeClause();
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected object name");
        }
        parseRecursiveClause();
    }
    // SHOW DEPENDENCIES [object_name]
    else if (matchContextual("DEPENDENCIES") || matchContextual("DEPENDENCY")) {
        stmt->show_type = ShowStmt::ShowType::DEPENDENCIES;
        parseScopeClause();
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected object name");
        }
        parseRecursiveClause();
    }
    // SHOW PACKAGE name
    else if (matchContextual("PACKAGE") || matchContextual("PACKAGES")) {
        stmt->show_type = ShowStmt::ShowType::PACKAGE;
        setUnifiedType("PACKAGE");
        parseScopeClause();
        if (check(TokenType::IDENTIFIER)) {
            stmt->name = expectIdentifier("Expected package name");
        }
        parseRecursiveClause();
    }
    // SHOW SQL DIALECT
    else if (matchContextual("SQL")) {
        expectContextual("DIALECT", "Expected DIALECT after SQL");
        stmt->show_type = ShowStmt::ShowType::SQL_DIALECT;
    }
    // SHOW TIME ZONE
    else if (matchContextual("TIME")) {
        expectContextual("ZONE", "Expected ZONE after TIME");
        stmt->show_type = ShowStmt::ShowType::VARIABLE;
        stmt->name = stringPool().intern("TIME_ZONE");
    }
    // SHOW VERSION
    else if (matchContextual("VERSION")) {
        stmt->show_type = ShowStmt::ShowType::VERSION;
    }
    // SHOW DATABASE
    else if (matchContextual("DATABASE")) {
        stmt->show_type = ShowStmt::ShowType::DATABASE;
    }
    // SHOW SYSTEM
    else if (matchContextual("SYSTEM")) {
        stmt->show_type = ShowStmt::ShowType::SYSTEM;
    }
    // SHOW METRICS
    else if (matchContextual("METRICS")) {
        stmt->show_type = ShowStmt::ShowType::METRICS;
    }
    // SHOW PARSER VERSION
    else if (matchContextual("PARSER")) {
        expectContextual("VERSION", "Expected VERSION after PARSER");
        stmt->show_type = ShowStmt::ShowType::VERSION;
    }
    // Default: SHOW variable_name
    else {
        stmt->show_type = ShowStmt::ShowType::VARIABLE;
        stmt->name = parseVariableName("Expected variable name or SHOW keyword");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

ShowStmt* Parser::parseDescribe() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<ShowStmt>();
    stmt->is_describe = true;

    auto parsePathString = [&](const char* message) -> StringPool::StringId {
        if (!canStartSchemaPath(state_)) {
            error(message);
            return StringPool::INVALID_ID;
        }
        SchemaPath path = parseSchemaPath(state_);
        return stringPool().intern(schemaPathToString(path, stringPool()));
    };

    auto parseDescribeMode = [&]() {
        if (matchContextual("FULL")) {
            stmt->describe_mode = ShowStmt::DescribeMode::FULL;
            return;
        }
        if (matchContextual("DDL")) {
            expectContextual("ONLY", "Expected ONLY after DDL");
            stmt->describe_mode = ShowStmt::DescribeMode::DDL_ONLY;
            return;
        }
        if (matchContextual("COMMENT")) {
            expectContextual("ONLY", "Expected ONLY after COMMENT");
            stmt->describe_mode = ShowStmt::DescribeMode::COMMENT_ONLY;
            return;
        }
        stmt->describe_mode = ShowStmt::DescribeMode::COMMENT_ONLY;
    };

    auto parseDescribeObjectType = [&](std::string& object_type) -> bool {
        struct Entry {
            const char* singular;
            const char* plural;
            const char* canonical;
        };
        static const Entry kEntries[] = {
            {"SCHEMA", "SCHEMAS", "SCHEMA"},
            {"TABLE", "TABLES", "TABLE"},
            {"VIEW", "VIEWS", "VIEW"},
            {"COLUMN", "COLUMNS", "COLUMN"},
            {"INDEX", "INDEXES", "INDEX"},
            {"SEQUENCE", "SEQUENCES", "SEQUENCE"},
            {"GENERATOR", "GENERATORS", "SEQUENCE"},
            {"DOMAIN", "DOMAINS", "DOMAIN"},
            {"FUNCTION", "FUNCTIONS", "FUNCTION"},
            {"PROCEDURE", "PROCEDURES", "PROCEDURE"},
            {"TRIGGER", "TRIGGERS", "TRIGGER"},
            {"PACKAGE", "PACKAGES", "PACKAGE"},
            {"ROLE", "ROLES", "ROLE"},
            {"DATABASE", "DATABASES", "DATABASE"},
            {"USER", "USERS", "USER"},
            {"GROUP", "GROUPS", "GROUP"},
        };
        for (const auto& entry : kEntries) {
            if (matchContextual(entry.singular) || matchContextual(entry.plural)) {
                object_type = entry.canonical;
                return true;
            }
        }
        return false;
    };

    // Canonical DESCRIBE:
    // DESCRIBE <object_name> OF <object_type> IN <path> [COMMENT ONLY|FULL|DDL ONLY]
    if (check(TokenType::IDENTIFIER) || check(TokenType::STRING_LITERAL)) {
        stmt->name = current().value.string_id;
        advance();

        if (matchContextual("OF")) {
            std::string object_type;
            if (!parseDescribeObjectType(object_type)) {
                error("Expected object type after DESCRIBE <name> OF");
                stmt->span = makeSpan(start);
                return stmt;
            }

            if (!(match(TokenType::KW_IN) || match(TokenType::KW_FROM))) {
                error("Expected IN/FROM after DESCRIBE <name> OF <type>");
                stmt->span = makeSpan(start);
                return stmt;
            }
            stmt->from_name = parsePathString("Expected schema/object path after DESCRIBE ... IN/FROM");
            stmt->show_type = ShowStmt::ShowType::OBJECTS;
            stmt->unified_metadata = true;
            stmt->metadata_object_type = stringPool().intern(object_type);
            parseDescribeMode();
            stmt->span = makeSpan(start);
            return stmt;
        }

        // Legacy MySQL-compatible DESCRIBE <table> [column]
        stmt->show_type = ShowStmt::ShowType::COLUMNS;
        stmt->from_name = stmt->name;
        if (check(TokenType::IDENTIFIER) || check(TokenType::STRING_LITERAL)) {
            stmt->like_pattern = current().value.string_id;
            advance();
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    error("Expected object name after DESCRIBE");

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// EXPLAIN Statement
// =============================================================================

ExplainStmt* Parser::parseExplain() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<ExplainStmt>();

    // Parse options: EXPLAIN [ANALYZE] [VERBOSE] [options] query
    // Options can be in parentheses: EXPLAIN (ANALYZE, VERBOSE, COSTS ON) query

    // Check for parenthesized options
    if (check(TokenType::LEFT_PAREN)) {
        advance();
        do {
            if (match(TokenType::KW_ANALYZE) || matchContextual("ANALYZE")) {
                stmt->analyze = true;
            } else if (matchContextual("VERBOSE")) {
                stmt->verbose = true;
            } else if (matchContextual("COSTS")) {
                // COSTS ON/OFF
                if (match(TokenType::KW_ON) || matchContextual("ON")) {
                    stmt->costs = true;
                } else if (matchContextual("OFF")) {
                    stmt->costs = false;
                }
            } else if (matchContextual("BUFFERS")) {
                stmt->buffers = true;
            } else if (matchContextual("WAL")) {
                stmt->wal = true;
                if (match(TokenType::KW_ON) || matchContextual("ON")) {
                    stmt->wal = true;
                } else if (matchContextual("OFF")) {
                    stmt->wal = false;
                }
            } else if (matchContextual("TIMING")) {
                // TIMING ON/OFF
                if (match(TokenType::KW_ON) || matchContextual("ON")) {
                    stmt->timing = true;
                } else if (matchContextual("OFF")) {
                    stmt->timing = false;
                }
            } else if (matchContextual("FORMAT")) {
                if (matchContextual("JSON")) {
                    stmt->format_json = true;
                } else if (matchContextual("XML")) {
                    stmt->format_xml = true;
                } else if (matchContextual("YAML")) {
                    stmt->format_yaml = true;
                } else if (matchContextual("TEXT")) {
                    // Default text format
                }
            } else {
                break;  // Unknown option
            }
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after EXPLAIN options");
    } else {
        // Non-parenthesized options (simpler form)
        if (match(TokenType::KW_ANALYZE) || matchContextual("ANALYZE")) {
            stmt->analyze = true;
        }
        if (matchContextual("VERBOSE")) {
            stmt->verbose = true;
        }
        bool keep_parsing = true;
        while (keep_parsing) {
            if (matchContextual("BUFFERS")) {
                stmt->buffers = true;
            } else if (matchContextual("WAL")) {
                stmt->wal = true;
            } else if (matchContextual("COSTS")) {
                if (match(TokenType::KW_ON) || matchContextual("ON")) {
                    stmt->costs = true;
                } else if (matchContextual("OFF")) {
                    stmt->costs = false;
                }
            } else if (matchContextual("TIMING")) {
                if (match(TokenType::KW_ON) || matchContextual("ON")) {
                    stmt->timing = true;
                } else if (matchContextual("OFF")) {
                    stmt->timing = false;
                }
            } else {
                keep_parsing = false;
            }
        }
    }

    // Parse the statement to explain
    if (match(TokenType::KW_SELECT)) {
        stmt->query = parseSelect();
    } else if (match(TokenType::KW_INSERT)) {
        stmt->query = parseInsert();
    } else if (match(TokenType::KW_UPDATE)) {
        stmt->query = parseUpdate();
    } else if (match(TokenType::KW_DELETE)) {
        stmt->query = parseDelete();
    } else {
        error("Expected SELECT, INSERT, UPDATE, or DELETE after EXPLAIN");
        return nullptr;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// ANALYZE Statement
// =============================================================================

AnalyzeStmt* Parser::parseAnalyze() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AnalyzeStmt>();

    if (matchContextual("VERBOSE")) {
        stmt->verbose = true;
    }

    auto parse_sample_rate = [&](const char* error_prefix) {
        if (check(TokenType::INTEGER_LITERAL)) {
            stmt->sample_rate = static_cast<double>(current().value.int_value);
            advance();
            stmt->has_sample = true;
            return;
        }
        if (check(TokenType::FLOAT_LITERAL)) {
            stmt->sample_rate = current().value.float_value;
            advance();
            stmt->has_sample = true;
            return;
        }
        error(std::string(error_prefix) + " sample rate");
    };

    if (matchContextual("INDEX")) {
        stmt->target = AnalyzeStmt::AnalyzeTarget::INDEX;
        stmt->index_path = parseSchemaPath(state_);
        if (stmt->index_path.isEmpty()) {
            error("Expected index name after ANALYZE INDEX");
        }

        if (match(TokenType::KW_WITH)) {
            expect(TokenType::LEFT_PAREN, "Expected '(' after WITH");
            while (!check(TokenType::RIGHT_PAREN) &&
                   !check(TokenType::SEMICOLON) &&
                   !check(TokenType::END_OF_FILE)) {
                if (!isIdentifier()) {
                    error("Expected option name in ANALYZE INDEX WITH clause");
                    break;
                }
                auto option_name = stringPool().get(current().value.string_id);
                advance();
                expect(TokenType::EQUAL, "Expected '=' after option name");
                if (caseInsensitiveEquals(option_name, "SAMPLE_RATE")) {
                    parse_sample_rate("Expected numeric");
                } else {
                    error("Unknown ANALYZE INDEX option");
                    break;
                }
                if (!match(TokenType::COMMA)) {
                    break;
                }
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after ANALYZE INDEX options");
        } else if (matchContextual("SAMPLE")) {
            parse_sample_rate("Expected numeric");
        }

        stmt->span = makeSpan(start);
        return stmt;
    }

    stmt->target = AnalyzeStmt::AnalyzeTarget::TABLE;
    stmt->table_path = parseSchemaPath(state_);
    if (stmt->table_path.isEmpty()) {
        error("Expected table name after ANALYZE");
    }

    bool parsed_paren = false;
    while (!isAtEnd()) {
        if (check(TokenType::LEFT_PAREN)) {
            if (parsed_paren || stmt->has_column) {
                error("ANALYZE supports only one column");
                break;
            }
            parsed_paren = true;
            advance();
            if (!check(TokenType::RIGHT_PAREN)) {
                stmt->column_name = expectIdentifier("Expected column name");
                stmt->has_column = true;
                if (match(TokenType::COMMA)) {
                    error("ANALYZE supports only one column");
                    while (!check(TokenType::RIGHT_PAREN) && !isAtEnd()) {
                        advance();
                    }
                }
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after column list");
            continue;
        }

        if (matchContextual("COLUMN")) {
            if (stmt->has_column) {
                error("ANALYZE supports only one column");
            }
            stmt->column_name = expectIdentifier("Expected column name after COLUMN");
            stmt->has_column = true;
            continue;
        }

        if (matchContextual("SAMPLE")) {
            if (stmt->has_sample) {
                error("SAMPLE specified more than once");
            }
            parse_sample_rate("Expected numeric");
            continue;
        }

        break;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseSecurityLabel() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    std::string provider;
    if (matchContextual("FOR")) {
        if (check(TokenType::STRING_LITERAL)) {
            provider = std::string(stringPool().get(current().value.string_id));
            advance();
        } else if (isIdentifier()) {
            provider = std::string(stringPool().get(current().value.string_id));
            advance();
        } else {
            error("Expected provider after SECURITY LABEL FOR");
        }
    }

    expect(TokenType::KW_ON, "Expected ON in SECURITY LABEL");
    StringPool::StringId object_type = expectIdentifier("Expected object type after SECURITY LABEL ON");
    SchemaPath object_path = parseSchemaPath(state_);
    if (object_path.isEmpty()) {
        error("Expected object path after SECURITY LABEL ON <object_type>");
    }
    expect(TokenType::KW_IS, "Expected IS in SECURITY LABEL");

    std::string label_text;
    if (match(TokenType::KW_NULL)) {
        label_text = "NULL";
    } else if (check(TokenType::STRING_LITERAL)) {
        label_text = std::string(stringPool().get(current().value.string_id));
        advance();
    } else {
        error("Expected string literal or NULL in SECURITY LABEL");
    }

    std::string payload;
    if (!provider.empty()) {
        payload.append("provider=");
        payload.append(provider);
        payload.push_back(';');
    }
    payload.append("object_type=");
    payload.append(std::string(stringPool().get(object_type)));
    payload.push_back(';');
    payload.append("object=");
    payload.append(schemaPathToString(object_path, stringPool()));
    payload.push_back(';');
    payload.append("label=");
    payload.append(label_text);

    stmt->name = stringPool().intern("security.label.set");
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

AlterIndexStmt* Parser::parseValidateIndex() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterIndexStmt>();
    stmt->action = AlterIndexAction::DIAGNOSTIC_SCAN;

    if (!matchContextual("INDEX")) {
        error("Expected INDEX after VALIDATE");
        stmt->span = makeSpan(start);
        return stmt;
    }

    stmt->index_path = parseSchemaPath(state_);
    if (stmt->index_path.isEmpty()) {
        error("Expected index name after VALIDATE INDEX");
        stmt->span = makeSpan(start);
        return stmt;
    }
    if (stmt->index_path.components.size() < 2) {
        errorCode("PRS_0505",
                  "VALIDATE INDEX requires explicit parent-qualified index reference");
    }

    if (match(TokenType::KW_WITH)) {
        expect(TokenType::LEFT_PAREN, "Expected '(' after WITH");
        while (!check(TokenType::RIGHT_PAREN) &&
               !check(TokenType::SEMICOLON) &&
               !check(TokenType::END_OF_FILE)) {
            auto option_name = expectIdentifier("Expected index option name");
            expect(TokenType::EQUAL, "Expected '=' after index option name");

            StringPool::StringId option_value = StringPool::INVALID_ID;
            if (check(TokenType::INTEGER_LITERAL)) {
                option_value = stringPool().intern(std::to_string(current().value.int_value));
                advance();
            } else if (check(TokenType::FLOAT_LITERAL)) {
                option_value = stringPool().intern(std::to_string(current().value.float_value));
                advance();
            } else if (check(TokenType::STRING_LITERAL) || isIdentifier()) {
                option_value = current().value.string_id;
                advance();
            } else if (match(TokenType::KW_TRUE)) {
                option_value = stringPool().intern("true");
            } else if (match(TokenType::KW_FALSE)) {
                option_value = stringPool().intern("false");
            } else {
                error("Expected scalar value for index option");
            }

            if (option_name != StringPool::INVALID_ID && option_value != StringPool::INVALID_ID) {
                stmt->option_assignments.push_back({option_name, option_value});
            }

            if (!match(TokenType::COMMA)) {
                break;
            }
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after index options");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// SWEEP DATABASE Statement
// =============================================================================

SweepDatabaseStmt* Parser::parseSweep() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<SweepDatabaseStmt>();

    if (!expectContextual("DATABASE", "Expected DATABASE after SWEEP")) {
        stmt->span = makeSpan(start);
        return stmt;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseResyncReplicationChannel() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    expectContextual("REPLICATION", "Expected REPLICATION after RESYNC");
    expectContextual("CHANNEL", "Expected CHANNEL after RESYNC REPLICATION");
    StringPool::StringId channel_name = expectIdentifier("Expected replication channel name");
    std::string channel = std::string(stringPool().get(channel_name));
    std::string payload = captureStatementBody();

    stmt->name = stringPool().intern("replication.channel.resync." + channel);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseDocPathFilterSurface() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<DocPathFilterStmt>();

    auto parse_u64 = [&](const char* label, uint64_t& out) -> bool {
        if (!check(TokenType::INTEGER_LITERAL)) {
            errorCode("PRS_0504", std::string("Expected unsigned integer for ").append(label));
            return false;
        }
        int64_t raw = current().value.int_value;
        advance();
        if (raw < 0) {
            errorCode("PRS_0504", std::string("Expected non-negative integer for ").append(label));
            return false;
        }
        out = static_cast<uint64_t>(raw);
        return true;
    };

    auto parse_cmp = [&](uint8_t& out) -> bool {
        if (match(TokenType::EQUAL)) {
            out = 0;  // CMP_EQ
            return true;
        }
        if (match(TokenType::NOT_EQUAL)) {
            out = 1;  // CMP_NE
            return true;
        }
        if (match(TokenType::LESS_THAN)) {
            out = 2;  // CMP_LT
            return true;
        }
        if (match(TokenType::LESS_EQUAL)) {
            out = 3;  // CMP_LE
            return true;
        }
        if (match(TokenType::GREATER_THAN)) {
            out = 4;  // CMP_GT
            return true;
        }
        if (match(TokenType::GREATER_EQUAL)) {
            out = 5;  // CMP_GE
            return true;
        }
        if (!isIdentifier()) {
            errorCode("PRS_0504", "Expected comparison operator");
            return false;
        }
        std::string symbol = std::string(stringPool().get(current().value.string_id));
        advance();
        for (char& c : symbol) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (symbol == "EQ") out = 0;
        else if (symbol == "NE") out = 1;
        else if (symbol == "LT") out = 2;
        else if (symbol == "LE") out = 3;
        else if (symbol == "GT") out = 4;
        else if (symbol == "GE") out = 5;
        else if (symbol == "EXISTS") out = 6;
        else if (symbol == "NOT_EXISTS") out = 7;
        else {
            errorCode("PRS_0504", "Unknown DOC PATH FILTER comparison operator");
            return false;
        }
        return true;
    };

    if (matchContextual("DOC")) {
        expectContextual("PATH", "Expected PATH after DOC");
        expectContextual("FILTER", "Expected FILTER after DOC PATH");
        if (!matchContextual("PATH_ID")) {
            errorCode("PRS_0504", "Expected PATH_ID in DOC PATH FILTER statement form");
        }
        parse_u64("PATH_ID", stmt->path_expr);
        if (!matchContextual("OP")) {
            errorCode("PRS_0504", "Expected OP in DOC PATH FILTER statement form");
        }
        parse_cmp(stmt->compare_op);
        if (!matchContextual("VALUE_REF")) {
            errorCode("PRS_0504", "Expected VALUE_REF in DOC PATH FILTER statement form");
        }
        parse_u64("VALUE_REF", stmt->value_expr);
    } else {
        errorCode("PRS_0505", "Unsupported DOC PATH FILTER surface");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseTimeBucketAggSurface() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<TsBucketAggStmt>();

    auto parse_u64 = [&](const char* label, uint64_t& out) -> bool {
        if (!check(TokenType::INTEGER_LITERAL)) {
            errorCode("PRS_0504", std::string("Expected unsigned integer for ").append(label));
            return false;
        }
        int64_t raw = current().value.int_value;
        advance();
        if (raw < 0) {
            errorCode("PRS_0504", std::string("Expected non-negative integer for ").append(label));
            return false;
        }
        out = static_cast<uint64_t>(raw);
        return true;
    };

    auto parse_agg_refs = [&]() {
        if (!expect(TokenType::LEFT_PAREN, "Expected '(' before aggregate reference list")) {
            return;
        }
        do {
            uint64_t ref = 0;
            if (parse_u64("aggregate reference", ref)) {
                stmt->agg_refs.push_back(ref);
            }
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after aggregate reference list");
        if (stmt->agg_refs.empty()) {
            errorCode("PRS_0504", "Aggregate reference list must not be empty");
        }
    };

    if (matchContextual("TS")) {
        expectContextual("BUCKET", "Expected BUCKET after TS");
        expectContextual("AGG", "Expected AGG after TS BUCKET");
        if (!matchContextual("TIME_EXPR")) {
            errorCode("PRS_0504", "Expected TIME_EXPR in TS BUCKET AGG statement form");
        }
        parse_u64("TIME_EXPR", stmt->time_expr);
        if (!matchContextual("BUCKET_NS")) {
            errorCode("PRS_0504", "Expected BUCKET_NS in TS BUCKET AGG statement form");
        }
        parse_u64("BUCKET_NS", stmt->bucket_size);
        if (!matchContextual("AGG_REFS")) {
            errorCode("PRS_0504", "Expected AGG_REFS in TS BUCKET AGG statement form");
        }
        parse_agg_refs();
    } else {
        errorCode("PRS_0505", "Unsupported TS BUCKET AGG surface");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseSearchDslSurface() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<SearchQueryDslStmt>();

    auto make_alter_system = [&](const std::string& key,
                                 const std::string& canonical_sql) -> Statement* {
        auto* alter = arena_.create<AlterSystemStmt>();
        alter->name = stringPool().intern(key);
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(canonical_sql);
        alter->value = lit;
        alter->span = makeSpan(start);
        return alter;
    };

    auto parse_u64 = [&](const char* label, uint64_t& out) -> bool {
        if (!check(TokenType::INTEGER_LITERAL)) {
            errorCode("PRS_0504", std::string("Expected unsigned integer for ").append(label));
            return false;
        }
        int64_t raw = current().value.int_value;
        advance();
        if (raw < 0) {
            errorCode("PRS_0504", std::string("Expected non-negative integer for ").append(label));
            return false;
        }
        out = static_cast<uint64_t>(raw);
        return true;
    };

    auto parse_scalar_text = [&](const char* label) -> std::string {
        if (check(TokenType::STRING_LITERAL) || isIdentifier()) {
            std::string value = std::string(stringPool().get(current().value.string_id));
            advance();
            return value;
        }
        if (check(TokenType::INTEGER_LITERAL)) {
            std::string value = std::to_string(current().value.int_value);
            advance();
            return value;
        }
        errorCode("PRS_0504", std::string("Expected scalar value for ").append(label));
        return {};
    };

    auto parse_enum = [&](const char* label, std::initializer_list<const char*> allowed) -> std::string {
        if (!isIdentifier()) {
            errorCode("PRS_0504", std::string("Expected identifier for ").append(label));
            return {};
        }
        std::string value = std::string(stringPool().get(current().value.string_id));
        advance();
        for (char& c : value) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        for (const char* allowed_value : allowed) {
            if (value == allowed_value) {
                return value;
            }
        }
        errorCode("PRS_0504", std::string("Unsupported ").append(label).append(" value"));
        return {};
    };

    auto parse_scorer = [&]() {
        if (!isIdentifier()) {
            errorCode("PRS_0504", "Expected scorer identifier");
            return;
        }
        std::string scorer = std::string(stringPool().get(current().value.string_id));
        advance();
        for (char& c : scorer) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (scorer == "BM25") stmt->scorer_id = 1;
        else if (scorer == "TFIDF") stmt->scorer_id = 2;
        else if (scorer == "DFR") stmt->scorer_id = 3;
        else errorCode("PRS_0504", "Unknown SEARCH DSL scorer");
    };

    if (matchContextual("SEARCH")) {
        if (matchContextual("QUERY")) {
            expectContextual("DSL", "Expected DSL after SEARCH QUERY");
            if (!matchContextual("TARGET_INDEX")) {
                errorCode("PRS_0504", "Expected TARGET_INDEX in SEARCH QUERY DSL statement form");
            }
            parse_u64("TARGET_INDEX", stmt->target_index);
            if (!matchContextual("PAYLOAD")) {
                errorCode("PRS_0504", "Expected PAYLOAD in SEARCH QUERY DSL statement form");
            }
            if (!check(TokenType::STRING_LITERAL)) {
                errorCode("PRS_0504", "Expected JSON payload string literal");
            } else {
                stmt->dsl_payload_json = current().value.string_id;
                advance();
            }
            if (matchContextual("SCORER")) {
                parse_scorer();
            }
        } else if (matchContextual("DSL")) {
            if (!check(TokenType::STRING_LITERAL)) {
                errorCode("PRS_0504", "Expected JSON payload string literal after SEARCH DSL");
            } else {
                stmt->dsl_payload_json = current().value.string_id;
                advance();
            }
            if (!match(TokenType::KW_ON) && !matchContextual("ON")) {
                errorCode("PRS_0505", "Expected ON in SEARCH DSL clause form");
            }
            if (!matchContextual("INDEX")) {
                errorCode("PRS_0505", "Expected INDEX in SEARCH DSL clause form");
            }
            parse_u64("INDEX", stmt->target_index);
            if (matchContextual("SCORER")) {
                parse_scorer();
            }
        } else if (match(TokenType::KW_JOIN) || matchContextual("JOIN")) {
            expectContextual("FIELD", "Expected FIELD after SEARCH JOIN");
            expectContextual("MAPPING", "Expected MAPPING after SEARCH JOIN FIELD");
            if (!matchContextual("INDEX")) {
                errorCode("PRS_0504", "Expected INDEX in SEARCH JOIN FIELD MAPPING statement form");
            }
            uint64_t target_index = 0;
            parse_u64("INDEX", target_index);
            if (!matchContextual("FIELD")) {
                errorCode("PRS_0504", "Expected FIELD in SEARCH JOIN FIELD MAPPING statement form");
            }
            std::string field_name = parse_scalar_text("FIELD");
            if (!matchContextual("PARENT")) {
                errorCode("PRS_0504", "Expected PARENT in SEARCH JOIN FIELD MAPPING statement form");
            }
            std::string parent_type = parse_scalar_text("PARENT");
            if (!matchContextual("CHILD")) {
                errorCode("PRS_0504", "Expected CHILD in SEARCH JOIN FIELD MAPPING statement form");
            }
            std::string child_type = parse_scalar_text("CHILD");
            std::string routing = "REQUIRED";
            if (matchContextual("ROUTING")) {
                routing = parse_enum("ROUTING", {"REQUIRED", "OPTIONAL"});
            }
            return make_alter_system(
                "search.join_field.mapping",
                "SEARCH JOIN FIELD MAPPING INDEX " + std::to_string(target_index) +
                    " FIELD " + field_name +
                    " PARENT " + parent_type +
                    " CHILD " + child_type +
                    " ROUTING " + routing);
        } else if (matchContextual("PERCOLATOR")) {
            expectContextual("FIELD", "Expected FIELD after SEARCH PERCOLATOR");
            if (!matchContextual("INDEX")) {
                errorCode("PRS_0504", "Expected INDEX in SEARCH PERCOLATOR FIELD statement form");
            }
            uint64_t target_index = 0;
            parse_u64("INDEX", target_index);
            if (!matchContextual("FIELD")) {
                errorCode("PRS_0504", "Expected FIELD in SEARCH PERCOLATOR FIELD statement form");
            }
            std::string field_name = parse_scalar_text("FIELD");
            std::string parser_mode = "STRICT";
            if (matchContextual("QUERY_PARSER")) {
                parser_mode = parse_enum("QUERY_PARSER", {"STRICT", "SIMPLE"});
            }
            return make_alter_system(
                "search.percolator.field",
                "SEARCH PERCOLATOR FIELD INDEX " + std::to_string(target_index) +
                    " FIELD " + field_name +
                    " QUERY_PARSER " + parser_mode);
        } else {
            errorCode("PRS_0505", "Unsupported SEARCH surface");
        }
    } else if (matchContextual("DSL")) {
        if (!check(TokenType::STRING_LITERAL)) {
            errorCode("PRS_0504", "Expected JSON payload string literal after SEARCH DSL");
        } else {
            stmt->dsl_payload_json = current().value.string_id;
            advance();
        }
        if (!match(TokenType::KW_ON) && !matchContextual("ON")) {
            errorCode("PRS_0505", "Expected ON in SEARCH DSL clause form");
        }
        if (!matchContextual("INDEX")) {
            errorCode("PRS_0505", "Expected INDEX in SEARCH DSL clause form");
        }
        parse_u64("INDEX", stmt->target_index);
        if (matchContextual("SCORER")) {
            parse_scorer();
        }
    } else if (match(TokenType::KW_JOIN) || matchContextual("JOIN")) {
        expectContextual("FIELD", "Expected FIELD after JOIN");
        std::string field_name = parse_scalar_text("FIELD");
        if (!match(TokenType::KW_ON) && !matchContextual("ON")) {
            errorCode("PRS_0505", "Expected ON in JOIN FIELD clause form");
        }
        if (!matchContextual("INDEX")) {
            errorCode("PRS_0505", "Expected INDEX in JOIN FIELD clause form");
        }
        uint64_t target_index = 0;
        parse_u64("INDEX", target_index);
        if (!matchContextual("PARENT")) {
            errorCode("PRS_0504", "Expected PARENT in JOIN FIELD clause form");
        }
        std::string parent_type = parse_scalar_text("PARENT");
        if (!matchContextual("CHILD")) {
            errorCode("PRS_0504", "Expected CHILD in JOIN FIELD clause form");
        }
        std::string child_type = parse_scalar_text("CHILD");
        std::string routing = "REQUIRED";
        if (matchContextual("ROUTING")) {
            routing = parse_enum("ROUTING", {"REQUIRED", "OPTIONAL"});
        }
        return make_alter_system(
            "search.join_field.mapping",
            "SEARCH JOIN FIELD MAPPING INDEX " + std::to_string(target_index) +
                " FIELD " + field_name +
                " PARENT " + parent_type +
                " CHILD " + child_type +
                " ROUTING " + routing);
    } else if (matchContextual("PERCOLATOR")) {
        expectContextual("FIELD", "Expected FIELD after PERCOLATOR");
        std::string field_name = parse_scalar_text("FIELD");
        if (!match(TokenType::KW_ON) && !matchContextual("ON")) {
            errorCode("PRS_0505", "Expected ON in PERCOLATOR FIELD clause form");
        }
        if (!matchContextual("INDEX")) {
            errorCode("PRS_0505", "Expected INDEX in PERCOLATOR FIELD clause form");
        }
        uint64_t target_index = 0;
        parse_u64("INDEX", target_index);
        std::string parser_mode = "STRICT";
        if (matchContextual("PARSER")) {
            parser_mode = parse_enum("PARSER", {"STRICT", "SIMPLE"});
        }
        return make_alter_system(
            "search.percolator.field",
            "SEARCH PERCOLATOR FIELD INDEX " + std::to_string(target_index) +
                " FIELD " + field_name +
                " QUERY_PARSER " + parser_mode);
    } else {
        errorCode("PRS_0505", "Unsupported SEARCH DSL surface");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAdminControlSurface(const char* command_keyword) {
    SourceLocation start = previous().span.start;
    auto* stmt = arena_.create<AlterSystemStmt>();

    auto parse_payload_literal = [&]() -> Expression* {
        std::string payload = captureStatementBody();
        if (payload.empty()) {
            return nullptr;
        }
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(payload);
        return lit;
    };

    std::string key;
    std::string keyword = command_keyword ? command_keyword : "";
    if (keyword == "BACKUP") {
        matchContextual("DATABASE");
        key = "admin.backup";
    } else if (keyword == "RESTORE") {
        matchContextual("DATABASE");
        key = "admin.restore";
    } else if (keyword == "VALIDATE") {
        matchContextual("DATABASE");
        key = "admin.validate";
    } else if (keyword == "CHECKPOINT") {
        matchContextual("DATABASE");
        key = "admin.checkpoint";
    } else {
        errorCode("PRS_0505", "Unsupported admin control command");
        stmt->span = makeSpan(start);
        return stmt;
    }

    stmt->name = stringPool().intern(key);
    stmt->value = parse_payload_literal();
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreateClusterControl() {
    SourceLocation start = previous().span.start;
    auto* stmt = arena_.create<AlterSystemStmt>();

    auto make_payload_literal = [&](const std::string& payload) -> Expression* {
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(payload);
        return lit;
    };

    if (canStartSchemaPath(state_) &&
        !checkContextual("WORKLOAD") &&
        !checkContextual("ADMISSION")) {
        SchemaPath cluster_path = parseSchemaPath(state_);
        if (cluster_path.isEmpty()) {
            error("Expected cluster name");
            stmt->span = makeSpan(start);
            return stmt;
        }
        std::string cluster_name = schemaPathToString(cluster_path, stringPool());
        std::string payload = captureStatementBody();
        stmt->name = stringPool().intern("cluster.ddl.create." + cluster_name);
        if (!payload.empty()) {
            stmt->value = make_payload_literal(payload);
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    std::string family;
    if (matchContextual("WORKLOAD")) {
        if (matchContextual("CLASS")) {
            family = "workload_class";
        } else if (matchContextual("ROUTE")) {
            family = "workload_route";
        } else {
            errorCode("PRS_0505", "Expected CLASS or ROUTE after CREATE CLUSTER WORKLOAD");
        }
    } else if (matchContextual("ADMISSION")) {
        if (matchContextual("POLICY")) {
            family = "admission_policy";
        } else if (matchContextual("BINDING")) {
            family = "admission_binding";
        } else {
            errorCode("PRS_0505", "Expected POLICY or BINDING after CREATE CLUSTER ADMISSION");
        }
    } else {
        errorCode("PRS_0505", "Expected WORKLOAD or ADMISSION after CREATE CLUSTER");
    }

    StringPool::StringId object_name = expectIdentifier("Expected cluster object name");
    std::string name = std::string(stringPool().get(object_name));
    std::string payload = captureStatementBody();

    if (payload.empty()) {
        errorCode("PRS_0504", "CREATE CLUSTER control requires configuration payload");
    }

    stmt->name = stringPool().intern("cluster." + family + ".create." + name);
    stmt->value = make_payload_literal(payload);
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAlterClusterControl() {
    SourceLocation start = previous().span.start;
    auto* stmt = arena_.create<AlterSystemStmt>();

    auto make_payload_literal = [&](const std::string& payload) -> Expression* {
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(payload);
        return lit;
    };

    if (canStartSchemaPath(state_) &&
        !checkContextual("SET") &&
        !checkContextual("WORKLOAD") &&
        !checkContextual("ADMISSION")) {
        SchemaPath cluster_path = parseSchemaPath(state_);
        if (cluster_path.isEmpty()) {
            error("Expected cluster name");
            stmt->span = makeSpan(start);
            return stmt;
        }

        std::string cluster_name = schemaPathToString(cluster_path, stringPool());
        auto finalize_stmt = [&](const char* action, std::string payload) -> Statement* {
            stmt->name = stringPool().intern(std::string("cluster.ddl.") + action + "." + cluster_name);
            if (!payload.empty()) {
                stmt->value = make_payload_literal(payload);
            }
            stmt->span = makeSpan(start);
            return stmt;
        };

        if (match(TokenType::KW_SET) || matchContextual("SET")) {
            if (matchContextual("STATE")) {
                if (!isIdentifier()) {
                    error("Expected cluster state after SET STATE");
                    return finalize_stmt("set_state", "");
                }
                std::string state = std::string(stringPool().get(current().value.string_id));
                std::string state_upper = toUpperAscii(state);
                if (state_upper != "ONLINE" && state_upper != "OFFLINE" &&
                    state_upper != "DEGRADED" && state_upper != "MAINTENANCE") {
                    errorCode("PRS_0504", "Unknown cluster state");
                }
                advance();
                std::string payload = state;
                std::string tail = captureStatementBody();
                if (!tail.empty()) {
                    payload.push_back(' ');
                    payload.append(tail);
                }
                return finalize_stmt("set_state", payload);
            }
            if (!check(TokenType::LEFT_PAREN)) {
                error("Expected '(' after SET in ALTER CLUSTER");
                return finalize_stmt("set", "");
            }
            return finalize_stmt("set", captureStatementBody());
        }

        if (matchContextual("RESET")) {
            if (!check(TokenType::LEFT_PAREN)) {
                error("Expected '(' after RESET in ALTER CLUSTER");
                return finalize_stmt("reset", "");
            }
            return finalize_stmt("reset", captureStatementBody());
        }

        if (matchContextual("RENAME")) {
            expectContextual("TO", "Expected TO after RENAME");
            StringPool::StringId new_name = expectIdentifier("Expected new cluster name");
            return finalize_stmt("rename", std::string(stringPool().get(new_name)));
        }

        if (matchContextual("START")) {
            return finalize_stmt("start", captureStatementBody());
        }
        if (matchContextual("STOP")) {
            return finalize_stmt("stop", captureStatementBody());
        }
        if (matchContextual("REFRESH")) {
            if (match(TokenType::KW_WITH) || check(TokenType::LEFT_PAREN)) {
                return finalize_stmt("refresh", captureStatementBody());
            }
            return finalize_stmt("refresh", "");
        }

        errorCode("PRS_0505",
                  "Expected SET, RESET, RENAME, SET STATE, START, STOP, or REFRESH after ALTER CLUSTER <name>");
        stmt->span = makeSpan(start);
        return stmt;
    }

    std::string family;
    if (matchContextual("SET")) {
        expectContextual("STATE", "Expected STATE after ALTER CLUSTER SET");
        std::string payload = captureStatementBody();
        if (payload.empty()) {
            errorCode("PRS_0504", "ALTER CLUSTER SET STATE requires payload");
        }
        stmt->name = stringPool().intern("cluster.set_state");
        stmt->value = make_payload_literal(payload);
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("WORKLOAD")) {
        if (matchContextual("CLASS")) {
            family = "workload_class";
        } else if (matchContextual("ROUTE")) {
            family = "workload_route";
        } else {
            errorCode("PRS_0505", "Expected CLASS or ROUTE after ALTER CLUSTER WORKLOAD");
        }
    } else if (matchContextual("ADMISSION")) {
        if (matchContextual("POLICY")) {
            family = "admission_policy";
        } else if (matchContextual("BINDING")) {
            family = "admission_binding";
        } else {
            errorCode("PRS_0505", "Expected POLICY or BINDING after ALTER CLUSTER ADMISSION");
        }
    } else {
        errorCode("PRS_0505", "Expected WORKLOAD, ADMISSION, or SET after ALTER CLUSTER");
    }

    StringPool::StringId object_name = expectIdentifier("Expected cluster object name");
    std::string name = std::string(stringPool().get(object_name));
    std::string payload = captureStatementBody();
    if (payload.empty()) {
        errorCode("PRS_0504", "ALTER CLUSTER control requires action payload");
    }

    stmt->name = stringPool().intern("cluster." + family + ".alter." + name);
    stmt->value = make_payload_literal(payload);
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseDropClusterControl() {
    SourceLocation start = previous().span.start;
    auto* stmt = arena_.create<AlterSystemStmt>();

    auto make_payload_literal = [&](const std::string& payload) -> Expression* {
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(payload);
        return lit;
    };

    bool if_exists = false;
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        if_exists = true;
    }

    if (canStartSchemaPath(state_) &&
        !checkContextual("WORKLOAD") &&
        !checkContextual("ADMISSION")) {
        std::vector<SchemaPath> cluster_paths;
        do {
            cluster_paths.push_back(parseSchemaPath(state_));
        } while (match(TokenType::COMMA));

        bool cascade = false;
        bool restrict = false;
        if (matchContextual("CASCADE")) {
            cascade = true;
        } else if (matchContextual("RESTRICT")) {
            restrict = true;
        }

        std::string payload = captureStatementBody();
        std::string names_payload;
        for (size_t i = 0; i < cluster_paths.size(); ++i) {
            if (i > 0) {
                names_payload.push_back(',');
            }
            names_payload.append(schemaPathToString(cluster_paths[i], stringPool()));
        }

        if (cluster_paths.size() == 1 && !cascade && !restrict) {
            stmt->name = stringPool().intern("cluster.ddl.drop." + names_payload);
            if (if_exists || !payload.empty()) {
                if (if_exists) {
                    if (!payload.empty()) payload.insert(0, ";");
                    payload.insert(0, "IF_EXISTS=1");
                }
                stmt->value = make_payload_literal(payload);
            }
            stmt->span = makeSpan(start);
            return stmt;
        }

        std::string composite_payload = "NAMES=" + names_payload;
        if (if_exists) composite_payload.append(";IF_EXISTS=1");
        if (cascade) composite_payload.append(";CASCADE=1");
        if (restrict) composite_payload.append(";RESTRICT=1");
        if (!payload.empty()) {
            composite_payload.push_back(';');
            composite_payload.append(payload);
        }

        stmt->name = stringPool().intern("cluster.ddl.drop");
        stmt->value = make_payload_literal(composite_payload);
        stmt->span = makeSpan(start);
        return stmt;
    }

    std::string family;
    if (matchContextual("WORKLOAD")) {
        if (matchContextual("CLASS")) {
            family = "workload_class";
        } else if (matchContextual("ROUTE")) {
            family = "workload_route";
        } else {
            errorCode("PRS_0505", "Expected CLASS or ROUTE after DROP CLUSTER WORKLOAD");
        }
    } else if (matchContextual("ADMISSION")) {
        if (matchContextual("POLICY")) {
            family = "admission_policy";
        } else if (matchContextual("BINDING")) {
            family = "admission_binding";
        } else {
            errorCode("PRS_0505", "Expected POLICY or BINDING after DROP CLUSTER ADMISSION");
        }
    } else {
        errorCode("PRS_0505", "Expected WORKLOAD or ADMISSION after DROP CLUSTER");
    }

    StringPool::StringId object_name = expectIdentifier("Expected cluster object name");
    std::string name = std::string(stringPool().get(object_name));
    std::string payload = captureStatementBody();

    stmt->name = stringPool().intern("cluster." + family + ".drop." + name);
    if (!payload.empty()) {
        stmt->value = make_payload_literal(payload);
    }
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseClusterControlSurface() {
    SourceLocation start = previous().span.start;
    auto* stmt = arena_.create<AlterSystemStmt>();

    auto make_payload_literal = [&](const std::string& payload) -> Expression* {
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(payload);
        return lit;
    };

    if (match(TokenType::KW_SET) || matchContextual("SET")) {
        expectContextual("STATE", "Expected STATE after CLUSTER SET");
        std::string payload = captureStatementBody();
        if (payload.empty()) {
            errorCode("PRS_0504", "CLUSTER SET STATE requires payload");
        }
        stmt->name = stringPool().intern("cluster.set_state");
        stmt->value = make_payload_literal(payload);
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (match(TokenType::KW_SHOW) || matchContextual("SHOW")) {
        errorCode("PRS_0505",
                  "CLUSTER SHOW is not supported in v3; use SHOW CLUSTER ...");
        stmt->span = makeSpan(start);
        return stmt;
    }

    errorCode("PRS_0505", "Expected SET or SHOW after CLUSTER");
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseShowClusterControlSurface() {
    SourceLocation start = previous().span.start;
    auto* stmt = arena_.create<AlterSystemStmt>();

    auto make_payload_literal = [&](const std::string& payload) -> Expression* {
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(payload);
        return lit;
    };

    expectContextual("CLUSTER", "Expected CLUSTER after SHOW");
    if (matchContextual("STATE")) {
        stmt->name = stringPool().intern("cluster.show_state");
    } else if (matchContextual("ROUTING")) {
        expectContextual("PLAN", "Expected PLAN after SHOW CLUSTER ROUTING");
        stmt->name = stringPool().intern("cluster.show_routing_plan");
    } else if (matchContextual("ADMISSION")) {
        expectContextual("STATUS", "Expected STATUS after SHOW CLUSTER ADMISSION");
        stmt->name = stringPool().intern("cluster.show_admission_status");
    } else {
        errorCode("PRS_0505",
                  "Expected STATE, ROUTING PLAN, or ADMISSION STATUS after SHOW CLUSTER");
    }

    std::string payload = captureStatementBody();
    if (!payload.empty()) {
        stmt->value = make_payload_literal(payload);
    }
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCreateCubeControl() {
    SourceLocation start = previous().span.start;
    auto* stmt = arena_.create<AlterSystemStmt>();

    auto make_payload_literal = [&](const std::string& payload) -> Expression* {
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(payload);
        return lit;
    };

    bool if_not_exists = false;
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        if_not_exists = true;
    }

    SchemaPath cube_path = parseSchemaPath(state_);
    if (cube_path.isEmpty()) {
        error("Expected cube name after CREATE CUBE");
    }
    std::string cube_name = schemaPathToString(cube_path, stringPool());
    std::string payload = captureStatementBody();

    if (if_not_exists) {
        if (!payload.empty()) {
            payload.insert(0, ";");
        }
        payload.insert(0, "IF_NOT_EXISTS=1");
    }

    stmt->name = stringPool().intern("cube.ddl.create." + cube_name);
    stmt->value = make_payload_literal(payload);
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseAlterCubeControl() {
    SourceLocation start = previous().span.start;
    auto* stmt = arena_.create<AlterSystemStmt>();

    auto make_payload_literal = [&](const std::string& payload) -> Expression* {
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(payload);
        return lit;
    };

    SchemaPath cube_path = parseSchemaPath(state_);
    if (cube_path.isEmpty()) {
        error("Expected cube name after ALTER CUBE");
        stmt->span = makeSpan(start);
        return stmt;
    }
    std::string cube_name = schemaPathToString(cube_path, stringPool());

    auto finalize_stmt = [&](const char* action, std::string payload) -> Statement* {
        stmt->name = stringPool().intern(std::string("cube.ddl.") + action + "." + cube_name);
        if (!payload.empty()) {
            stmt->value = make_payload_literal(payload);
        }
        stmt->span = makeSpan(start);
        return stmt;
    };

    if (match(TokenType::KW_SET) || matchContextual("SET")) {
        if (!check(TokenType::LEFT_PAREN)) {
            error("Expected '(' after SET in ALTER CUBE");
            return finalize_stmt("set", "");
        }
        return finalize_stmt("set", captureStatementBody());
    }

    if (matchContextual("RESET")) {
        if (!check(TokenType::LEFT_PAREN)) {
            error("Expected '(' after RESET in ALTER CUBE");
            return finalize_stmt("reset", "");
        }
        return finalize_stmt("reset", captureStatementBody());
    }

    if (matchContextual("RENAME")) {
        expectContextual("TO", "Expected TO after RENAME");
        StringPool::StringId new_name = expectIdentifier("Expected new cube name");
        return finalize_stmt("rename", std::string(stringPool().get(new_name)));
    }

    if (matchContextual("START")) {
        return finalize_stmt("start", captureStatementBody());
    }
    if (matchContextual("STOP")) {
        return finalize_stmt("stop", captureStatementBody());
    }
    if (matchContextual("REBUILD")) {
        std::string payload = "REBUILD";
        if (match(TokenType::KW_WITH) || check(TokenType::LEFT_PAREN)) {
            std::string detail = captureStatementBody();
            if (!detail.empty()) {
                payload.push_back(' ');
                payload.append(detail);
            }
        } else if (isIdentifier()) {
            payload.push_back(' ');
            payload.append(std::string(stringPool().get(current().value.string_id)));
            advance();
        }
        return finalize_stmt("alter", payload);
    }
    if (matchContextual("REFRESH")) {
        if (match(TokenType::KW_WITH) || check(TokenType::LEFT_PAREN)) {
            return finalize_stmt("refresh", captureStatementBody());
        }
        return finalize_stmt("refresh", "");
    }

    errorCode("PRS_0505",
              "Expected SET, RESET, RENAME, START, STOP, REBUILD, or REFRESH after ALTER CUBE <name>");
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseDropCubeControl() {
    SourceLocation start = previous().span.start;
    auto* stmt = arena_.create<AlterSystemStmt>();

    auto make_payload_literal = [&](const std::string& payload) -> Expression* {
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(payload);
        return lit;
    };

    bool if_exists = false;
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF");
        if_exists = true;
    }

    std::vector<SchemaPath> cube_paths;
    do {
        cube_paths.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    bool cascade = false;
    bool restrict = false;
    if (matchContextual("CASCADE")) {
        cascade = true;
    } else if (matchContextual("RESTRICT")) {
        restrict = true;
    }

    std::string payload = captureStatementBody();
    std::string names_payload;
    for (size_t i = 0; i < cube_paths.size(); ++i) {
        if (i > 0) {
            names_payload.push_back(',');
        }
        names_payload.append(schemaPathToString(cube_paths[i], stringPool()));
    }

    if (cube_paths.size() == 1 && !cascade && !restrict) {
        if (if_exists) {
            if (!payload.empty()) {
                payload.insert(0, ";");
            }
            payload.insert(0, "IF_EXISTS=1");
        }
        stmt->name = stringPool().intern("cube.ddl.drop." + names_payload);
        if (!payload.empty()) {
            stmt->value = make_payload_literal(payload);
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    std::string composite_payload = "NAMES=" + names_payload;
    if (if_exists) composite_payload.append(";IF_EXISTS=1");
    if (cascade) composite_payload.append(";CASCADE=1");
    if (restrict) composite_payload.append(";RESTRICT=1");
    if (!payload.empty()) {
        composite_payload.push_back(';');
        composite_payload.append(payload);
    }

    stmt->name = stringPool().intern("cube.ddl.drop");
    stmt->value = make_payload_literal(composite_payload);
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseRefreshCubeControl() {
    SourceLocation start = previous().span.start;
    auto* stmt = arena_.create<AlterSystemStmt>();

    auto make_payload_literal = [&](const std::string& payload) -> Expression* {
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(payload);
        return lit;
    };

    expectContextual("CUBE", "Expected CUBE after REFRESH");
    StringPool::StringId cube_name_id = expectIdentifier("Expected cube name after REFRESH CUBE");
    std::string cube_name = std::string(stringPool().get(cube_name_id));
    std::string payload = captureStatementBody();

    stmt->name = stringPool().intern("cube.refresh." + cube_name);
    if (!payload.empty()) {
        stmt->value = make_payload_literal(payload);
    }
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseCubeControlSurface() {
    SourceLocation start = previous().span.start;
    auto* stmt = arena_.create<AlterSystemStmt>();

    auto make_payload_literal = [&](const std::string& payload) -> Expression* {
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(payload);
        return lit;
    };

    if (matchContextual("REFRESH")) {
        StringPool::StringId cube_name_id = expectIdentifier("Expected cube name after CUBE REFRESH");
        std::string cube_name = std::string(stringPool().get(cube_name_id));
        std::string payload = captureStatementBody();
        stmt->name = stringPool().intern("cube.refresh." + cube_name);
        if (!payload.empty()) {
            stmt->value = make_payload_literal(payload);
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (match(TokenType::KW_SHOW) || matchContextual("SHOW")) {
        std::string cube_name;
        if (matchContextual("STATS")) {
            if (isIdentifier()) {
                cube_name = std::string(stringPool().get(expectIdentifier("Expected cube name")));
            }
        } else if (isIdentifier()) {
            cube_name = std::string(stringPool().get(expectIdentifier("Expected cube name")));
            matchContextual("STATS");
        }

        stmt->name = stringPool().intern("cube.show_stats");
        std::string payload = captureStatementBody();
        if (!cube_name.empty()) {
            if (!payload.empty()) {
                payload.insert(0, " ");
            }
            payload.insert(0, cube_name);
        }
        if (!payload.empty()) {
            stmt->value = make_payload_literal(payload);
        }
        stmt->span = makeSpan(start);
        return stmt;
    }

    errorCode("PRS_0505", "Expected REFRESH or SHOW after CUBE");
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseShowCubeControlSurface() {
    SourceLocation start = previous().span.start;
    auto* stmt = arena_.create<AlterSystemStmt>();

    auto make_payload_literal = [&](const std::string& payload) -> Expression* {
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(payload);
        return lit;
    };

    expectContextual("CUBE", "Expected CUBE after SHOW");
    std::string cube_name;
    if (matchContextual("STATS")) {
        if (isIdentifier()) {
            cube_name = std::string(stringPool().get(expectIdentifier("Expected cube name")));
        }
    } else if (isIdentifier()) {
        cube_name = std::string(stringPool().get(expectIdentifier("Expected cube name")));
        matchContextual("STATS");
    }

    stmt->name = stringPool().intern("cube.show_stats");
    std::string payload = captureStatementBody();
    if (!cube_name.empty()) {
        if (!payload.empty()) {
            payload.insert(0, " ");
        }
        payload.insert(0, cube_name);
    }
    if (!payload.empty()) {
        stmt->value = make_payload_literal(payload);
    }
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseServiceChannelSurface() {
    SourceLocation start = previous().span.start;
    auto* stmt = arena_.create<AlterSystemStmt>();

    auto make_payload_literal = [&](const std::string& payload) -> Expression* {
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(payload);
        return lit;
    };

    expectContextual("CHANNEL", "Expected CHANNEL after SERVICE");
    if (matchContextual("BACKUP")) {
        stmt->name = stringPool().intern("service.channel.backup");
    } else if (matchContextual("EVENTS")) {
        stmt->name = stringPool().intern("service.channel.events");
    } else if (matchContextual("PROGRESS")) {
        stmt->name = stringPool().intern("service.channel.progress");
    } else {
        errorCode("PRS_0505", "Expected BACKUP, EVENTS, or PROGRESS after SERVICE CHANNEL");
    }

    std::string payload = captureStatementBody();
    if (!payload.empty()) {
        stmt->value = make_payload_literal(payload);
    }
    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseNoSqlSurface() {
    SourceLocation start = currentLocation();

    auto make_nosql_stmt = [&](const std::string& key, Expression* payload_expr) -> Statement* {
        auto* stmt = arena_.create<AlterSystemStmt>();
        stmt->name = stringPool().intern(key);
        stmt->value = payload_expr;
        stmt->span = makeSpan(start);
        return stmt;
    };

    auto parse_payload_expr = [&](const char* label) -> Expression* {
        if (check(TokenType::SEMICOLON) || isAtEnd()) {
            errorCode("PRS_0504", std::string("Expected payload expression for ").append(label));
            return nullptr;
        }
        return parseExpression();
    };

    auto parse_query_expr = [&](const char* label) -> Expression* {
        matchContextual("QUERY");
        return parse_payload_expr(label);
    };

    if (!matchContextual("REDIS")) {
        errorCode("PRS_0505", "Unsupported NoSQL surface");
        auto* fallback = arena_.create<AlterSystemStmt>();
        fallback->span = makeSpan(start);
        return fallback;
    }

    if (matchContextual("STRING")) {
        return make_nosql_stmt("nosql.redis.string", parse_query_expr("REDIS STRING"));
    }
    if (matchContextual("HASH")) {
        return make_nosql_stmt("nosql.redis.hash", parse_query_expr("REDIS HASH"));
    }
    if (matchContextual("LIST")) {
        return make_nosql_stmt("nosql.redis.list", parse_query_expr("REDIS LIST"));
    }
    if (match(TokenType::KW_SET) || matchContextual("SET")) {
        return make_nosql_stmt("nosql.redis.set", parse_query_expr("REDIS SET"));
    }
    if (matchContextual("ZSET")) {
        return make_nosql_stmt("nosql.redis.zset", parse_query_expr("REDIS ZSET"));
    }

    if (matchContextual("PUBSUB")) {
        errorCode("PRS_0505",
                  "REDIS PUBSUB alias is not supported in v3");
    } else {
        errorCode("PRS_0505", "Unsupported REDIS KV surface");
    }
    auto* fallback = arena_.create<AlterSystemStmt>();
    fallback->span = makeSpan(start);
    return fallback;
}

Statement* Parser::parseVectorAnnSurface() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<VectorAnnQueryStmt>();

    auto parse_u64 = [&](const char* label, uint64_t& out) -> bool {
        if (!check(TokenType::INTEGER_LITERAL)) {
            errorCode("PRS_0504", std::string("Expected unsigned integer for ").append(label));
            return false;
        }
        int64_t raw = current().value.int_value;
        advance();
        if (raw < 0) {
            errorCode("PRS_0504", std::string("Expected non-negative integer for ").append(label));
            return false;
        }
        out = static_cast<uint64_t>(raw);
        return true;
    };

    auto parse_metric = [&]() {
        if (!isIdentifier()) {
            errorCode("PRS_0504", "Expected metric identifier");
            return;
        }
        std::string metric = std::string(stringPool().get(current().value.string_id));
        advance();
        for (char& c : metric) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (metric == "L2") stmt->metric = 1;
        else if (metric == "COSINE") stmt->metric = 2;
        else if (metric == "DOT") stmt->metric = 3;
        else errorCode("PRS_0504", "Unknown vector metric");
    };

    if (matchContextual("VECTOR")) {
        expectContextual("ANN", "Expected ANN after VECTOR");
        expectContextual("QUERY", "Expected QUERY after VECTOR ANN");
        if (!matchContextual("INDEX")) {
            errorCode("PRS_0504", "Expected INDEX in VECTOR ANN QUERY statement form");
        }
        parse_u64("INDEX", stmt->vector_expr);
        if (!matchContextual("METRIC")) {
            errorCode("PRS_0504", "Expected METRIC in VECTOR ANN QUERY statement form");
        }
        parse_metric();
        if (!matchContextual("TOPK")) {
            errorCode("PRS_0504", "Expected TOPK in VECTOR ANN QUERY statement form");
        }
        parse_u64("TOPK", stmt->k);
        if (!matchContextual("EF_SEARCH")) {
            errorCode("PRS_0504", "Expected EF_SEARCH in VECTOR ANN QUERY statement form");
        }
        parse_u64("EF_SEARCH", stmt->ef_search);
    } else {
        errorCode("PRS_0505", "Unsupported VECTOR ANN surface");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseHybridBridgeSurface() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<HybridBridgeStmt>();

    auto parse_u64 = [&](const char* label, uint64_t& out) -> bool {
        if (!check(TokenType::INTEGER_LITERAL)) {
            errorCode("PRS_0504", std::string("Expected unsigned integer for ").append(label));
            return false;
        }
        int64_t raw = current().value.int_value;
        advance();
        if (raw < 0) {
            errorCode("PRS_0504", std::string("Expected non-negative integer for ").append(label));
            return false;
        }
        out = static_cast<uint64_t>(raw);
        return true;
    };

    auto parse_mode = [&]() {
        if (!isIdentifier()) {
            errorCode("PRS_0504", "Expected bridge mode identifier");
            return;
        }
        std::string mode = std::string(stringPool().get(current().value.string_id));
        advance();
        for (char& c : mode) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (mode == "HASH_SHUFFLE") stmt->bridge_mode = 1;
        else if (mode == "RANGE_SHUFFLE") stmt->bridge_mode = 2;
        else if (mode == "BROADCAST") stmt->bridge_mode = 3;
        else errorCode("PRS_0504", "Unknown hybrid bridge mode");
    };

    if (matchContextual("HYBRID")) {
        expectContextual("BRIDGE", "Expected BRIDGE after HYBRID");
        expectContextual("EXCHANGE", "Expected EXCHANGE after HYBRID BRIDGE");
        if (!matchContextual("SOURCE_TRACK")) {
            errorCode("PRS_0504", "Expected SOURCE_TRACK in HYBRID BRIDGE statement form");
        }
        parse_u64("SOURCE_TRACK", stmt->source_track);
        if (!matchContextual("TARGET_TRACK")) {
            errorCode("PRS_0504", "Expected TARGET_TRACK in HYBRID BRIDGE statement form");
        }
        parse_u64("TARGET_TRACK", stmt->target_track);
        if (!matchContextual("MODE")) {
            errorCode("PRS_0504", "Expected MODE in HYBRID BRIDGE statement form");
        }
        parse_mode();
    } else if (matchContextual("BRIDGE")) {
        expectContextual("SOURCE", "Expected SOURCE in BRIDGE clause form");
        parse_u64("SOURCE", stmt->source_track);
        expectContextual("TARGET", "Expected TARGET in BRIDGE clause form");
        parse_u64("TARGET", stmt->target_track);
        expectContextual("MODE", "Expected MODE in BRIDGE clause form");
        parse_mode();
    } else {
        errorCode("PRS_0505", "Unsupported HYBRID BRIDGE surface");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseGraphPathSurface() {
    SourceLocation start = currentLocation();

    auto make_alter_system = [&](const std::string& canonical_sql) -> Statement* {
        auto* stmt = arena_.create<AlterSystemStmt>();
        stmt->name = stringPool().intern("graph.path.quantified");
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(canonical_sql);
        stmt->value = lit;
        stmt->span = makeSpan(start);
        return stmt;
    };

    auto parse_u64 = [&](const char* label, uint64_t& out) -> bool {
        if (!check(TokenType::INTEGER_LITERAL)) {
            errorCode("PRS_0504", std::string("Expected unsigned integer for ").append(label));
            return false;
        }
        int64_t raw = current().value.int_value;
        advance();
        if (raw < 0) {
            errorCode("PRS_0504", std::string("Expected non-negative integer for ").append(label));
            return false;
        }
        out = static_cast<uint64_t>(raw);
        return true;
    };

    auto parse_scalar_text = [&](const char* label) -> std::string {
        if (check(TokenType::STRING_LITERAL) || isIdentifier()) {
            std::string value = std::string(stringPool().get(current().value.string_id));
            advance();
            return value;
        }
        if (check(TokenType::INTEGER_LITERAL)) {
            std::string value = std::to_string(current().value.int_value);
            advance();
            return value;
        }
        errorCode("PRS_0504", std::string("Expected scalar value for ").append(label));
        return {};
    };

    auto parse_cycle_policy = [&]() -> std::string {
        if (!isIdentifier()) {
            errorCode("PRS_0504", "Expected CYCLE_POLICY value");
            return {};
        }
        std::string value = std::string(stringPool().get(current().value.string_id));
        advance();
        for (char& c : value) {
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (value == "NO_REPEAT" || value == "ALLOW") {
            return value;
        }
        errorCode("PRS_0504", "Unsupported CYCLE_POLICY value");
        return {};
    };

    std::string pattern_ref;
    uint64_t min_hops = 0;
    uint64_t max_hops = 0;
    std::string cycle_policy = "NO_REPEAT";

    if (matchContextual("GRAPH")) {
        expectContextual("PATH", "Expected PATH after GRAPH");
        expectContextual("MATCH", "Expected MATCH after GRAPH PATH");
        if (!matchContextual("PATTERN")) {
            errorCode("PRS_0504", "Expected PATTERN in GRAPH PATH MATCH statement form");
        }
        pattern_ref = parse_scalar_text("PATTERN");
        if (!matchContextual("MIN_HOPS")) {
            errorCode("PRS_0504", "Expected MIN_HOPS in GRAPH PATH MATCH statement form");
        }
        parse_u64("MIN_HOPS", min_hops);
        if (!matchContextual("MAX_HOPS")) {
            errorCode("PRS_0504", "Expected MAX_HOPS in GRAPH PATH MATCH statement form");
        }
        parse_u64("MAX_HOPS", max_hops);
        if (max_hops < min_hops) {
            errorCode("PRS_0504", "MAX_HOPS must be greater than or equal to MIN_HOPS");
        }
        if (matchContextual("CYCLE_POLICY")) {
            cycle_policy = parse_cycle_policy();
        }
    } else if (matchContextual("MATCH")) {
        expectContextual("GRAPH", "Expected GRAPH after MATCH");
        expectContextual("PATH", "Expected PATH after MATCH GRAPH");
        pattern_ref = parse_scalar_text("path pattern");
        if (!matchContextual("HOPS")) {
            errorCode("PRS_0504", "Expected HOPS in MATCH GRAPH PATH clause form");
        }
        parse_u64("min hop", min_hops);
        if (!match(TokenType::DOUBLE_DOT)) {
            errorCode("PRS_0504", "Expected '..' in MATCH GRAPH PATH HOPS range");
        }
        parse_u64("max hop", max_hops);
        if (max_hops < min_hops) {
            errorCode("PRS_0504", "MAX_HOPS must be greater than or equal to MIN_HOPS");
        }
        if (matchContextual("NO")) {
            if (!matchContextual("CYCLES")) {
                errorCode("PRS_0504", "Expected CYCLES after NO");
            }
            cycle_policy = "NO_REPEAT";
        } else if (matchContextual("ALLOW")) {
            if (!matchContextual("CYCLES")) {
                errorCode("PRS_0504", "Expected CYCLES after ALLOW");
            }
            cycle_policy = "ALLOW";
        }
    } else {
        errorCode("PRS_0505", "Unsupported GRAPH PATH surface");
    }

    return make_alter_system(
        "GRAPH PATH MATCH PATTERN " + pattern_ref +
        " MIN_HOPS " + std::to_string(min_hops) +
        " MAX_HOPS " + std::to_string(max_hops) +
        " CYCLE_POLICY " + cycle_policy);
}

Statement* Parser::parseRedisLuaEvalSurface() {
    SourceLocation start = currentLocation();

    auto make_alter_system = [&](const std::string& canonical_sql) -> Statement* {
        auto* stmt = arena_.create<AlterSystemStmt>();
        stmt->name = stringPool().intern("redis.lua.eval");
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(canonical_sql);
        stmt->value = lit;
        stmt->span = makeSpan(start);
        return stmt;
    };

    auto parse_scalar_text = [&](const char* label) -> std::string {
        if (check(TokenType::STRING_LITERAL) || isIdentifier()) {
            std::string value = std::string(stringPool().get(current().value.string_id));
            advance();
            return value;
        }
        if (check(TokenType::INTEGER_LITERAL)) {
            std::string value = std::to_string(current().value.int_value);
            advance();
            return value;
        }
        errorCode("PRS_0504", std::string("Expected scalar value for ").append(label));
        return {};
    };

    auto parse_list = [&](const char* label) -> std::vector<std::string> {
        std::vector<std::string> values;
        expect(TokenType::LEFT_PAREN, std::string("Expected '(' before ").append(label).append(" list"));
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                values.push_back(parse_scalar_text(label));
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, std::string("Expected ')' after ").append(label).append(" list"));
        return values;
    };

    auto join_csv = [](const std::vector<std::string>& values) -> std::string {
        std::string out;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) out.push_back(',');
            out.append(values[i]);
        }
        return out;
    };

    std::string script_body;
    std::vector<std::string> keys;
    std::vector<std::string> args;
    std::string script_sha;
    uint64_t timeout_ms = 0;
    bool has_timeout = false;

    if (matchContextual("REDIS")) {
        expectContextual("LUA", "Expected LUA after REDIS");
        expectContextual("EVAL", "Expected EVAL after REDIS LUA");
        if (!matchContextual("SCRIPT")) {
            errorCode("PRS_0504", "Expected SCRIPT in REDIS LUA EVAL statement form");
        }
        script_body = parse_scalar_text("SCRIPT");
        if (!matchContextual("KEYS")) {
            errorCode("PRS_0504", "Expected KEYS in REDIS LUA EVAL statement form");
        }
        keys = parse_list("KEYS");
        if (!matchContextual("ARGS")) {
            errorCode("PRS_0504", "Expected ARGS in REDIS LUA EVAL statement form");
        }
        args = parse_list("ARGS");
        if (matchContextual("SHA")) {
            script_sha = parse_scalar_text("SHA");
        }
        if (matchContextual("TIMEOUT_MS")) {
            if (!check(TokenType::INTEGER_LITERAL) || current().value.int_value < 0) {
                errorCode("PRS_0504", "Expected non-negative TIMEOUT_MS");
            } else {
                timeout_ms = static_cast<uint64_t>(current().value.int_value);
                advance();
                has_timeout = true;
            }
        }
    } else {
        errorCode("PRS_0505", "Unsupported REDIS LUA EVAL surface");
    }

    std::string canonical =
        "REDIS LUA EVAL SCRIPT " + script_body +
        " KEYS (" + join_csv(keys) + ")" +
        " ARGS (" + join_csv(args) + ")";
    if (!script_sha.empty()) {
        canonical.append(" SHA ");
        canonical.append(script_sha);
    }
    if (has_timeout) {
        canonical.append(" TIMEOUT_MS ");
        canonical.append(std::to_string(timeout_ms));
    }
    return make_alter_system(canonical);
}

Statement* Parser::parseRedisStreamGroupSurface() {
    SourceLocation start = currentLocation();

    auto make_alter_system = [&](const std::string& key, const std::string& canonical_sql) -> Statement* {
        auto* stmt = arena_.create<AlterSystemStmt>();
        stmt->name = stringPool().intern(key);
        auto* lit = arena_.create<LiteralExpr>();
        lit->literal_type = LiteralType::STRING;
        lit->string_value = stringPool().intern(canonical_sql);
        stmt->value = lit;
        stmt->span = makeSpan(start);
        return stmt;
    };

    auto make_nosql_stmt = [&](const std::string& key, Expression* payload_expr) -> Statement* {
        auto* stmt = arena_.create<AlterSystemStmt>();
        stmt->name = stringPool().intern(key);
        stmt->value = payload_expr;
        stmt->span = makeSpan(start);
        return stmt;
    };

    auto parse_scalar_text = [&](const char* label) -> std::string {
        if (check(TokenType::STRING_LITERAL) || isIdentifier()) {
            std::string value = std::string(stringPool().get(current().value.string_id));
            advance();
            return value;
        }
        if (check(TokenType::INTEGER_LITERAL)) {
            std::string value = std::to_string(current().value.int_value);
            advance();
            return value;
        }
        errorCode("PRS_0504", std::string("Expected scalar value for ").append(label));
        return {};
    };

    auto parse_stream_id = [&](const char* label) -> std::string {
        if (check(TokenType::STAR)) {
            advance();
            return "*";
        }
        if (check(TokenType::STRING_LITERAL) || isIdentifier()) {
            std::string value = std::string(stringPool().get(current().value.string_id));
            advance();
            return value;
        }
        if (check(TokenType::INTEGER_LITERAL)) {
            int64_t first = current().value.int_value;
            if (first < 0) {
                errorCode("PRS_0504", std::string("Expected non-negative stream id component for ").append(label));
                return {};
            }
            advance();
            if (match(TokenType::MINUS)) {
                if (!check(TokenType::INTEGER_LITERAL) || current().value.int_value < 0) {
                    errorCode("PRS_0504", std::string("Expected non-negative stream id suffix for ").append(label));
                    return {};
                }
                int64_t second = current().value.int_value;
                advance();
                return std::to_string(first) + "-" + std::to_string(second);
            }
            return std::to_string(first);
        }
        errorCode("PRS_0504", std::string("Expected stream id value for ").append(label));
        return {};
    };

    auto parse_u64 = [&](const char* label, uint64_t& out) -> bool {
        if (!check(TokenType::INTEGER_LITERAL)) {
            errorCode("PRS_0504", std::string("Expected unsigned integer for ").append(label));
            return false;
        }
        int64_t raw = current().value.int_value;
        advance();
        if (raw < 0) {
            errorCode("PRS_0504", std::string("Expected non-negative integer for ").append(label));
            return false;
        }
        out = static_cast<uint64_t>(raw);
        return true;
    };

    auto parse_ids = [&]() -> std::vector<std::string> {
        std::vector<std::string> ids;
        expect(TokenType::LEFT_PAREN, "Expected '(' before IDS list");
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                ids.push_back(parse_stream_id("ID"));
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after IDS list");
        return ids;
    };

    auto join_csv = [](const std::vector<std::string>& values) -> std::string {
        std::string out;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i > 0) out.push_back(',');
            out.append(values[i]);
        }
        return out;
    };

    auto expect_group_keyword = [&](const char* message) -> bool {
        if (match(TokenType::KW_GROUP) || matchContextual("GROUP")) {
            return true;
        }
        error(message);
        return false;
    };

    if (matchContextual("REDIS")) {
        expectContextual("STREAM", "Expected STREAM after REDIS");

        if (!(check(TokenType::KW_GROUP) || checkContextual("GROUP"))) {
            matchContextual("QUERY");
            if (check(TokenType::SEMICOLON) || isAtEnd()) {
                errorCode("PRS_0504", "Expected payload expression for REDIS STREAM");
                auto* fallback = arena_.create<AlterSystemStmt>();
                fallback->span = makeSpan(start);
                return fallback;
            }
            return make_nosql_stmt("nosql.redis.stream", parseExpression());
        }

        expect_group_keyword("Expected GROUP after REDIS STREAM");

        if (match(TokenType::KW_CREATE) || matchContextual("CREATE")) {
            expectContextual("STREAM", "Expected STREAM in REDIS STREAM GROUP CREATE");
            std::string stream_name = parse_scalar_text("STREAM");
            expect_group_keyword("Expected GROUP in REDIS STREAM GROUP CREATE");
            std::string group_name = parse_scalar_text("GROUP");
            if (!matchContextual("START_ID")) {
                errorCode("PRS_0504", "Expected START_ID in REDIS STREAM GROUP CREATE");
            }
            std::string start_id = parse_stream_id("START_ID");
            return make_alter_system(
                "redis.stream.group.create",
                "REDIS STREAM GROUP CREATE STREAM " + stream_name +
                    " GROUP " + group_name +
                    " START_ID " + start_id);
        }

        if (matchContextual("READ")) {
            expectContextual("STREAM", "Expected STREAM in REDIS STREAM GROUP READ");
            std::string stream_name = parse_scalar_text("STREAM");
            expect_group_keyword("Expected GROUP in REDIS STREAM GROUP READ");
            std::string group_name = parse_scalar_text("GROUP");
            expectContextual("CONSUMER", "Expected CONSUMER in REDIS STREAM GROUP READ");
            std::string consumer_name = parse_scalar_text("CONSUMER");
            if (!matchContextual("COUNT")) {
                errorCode("PRS_0504", "Expected COUNT in REDIS STREAM GROUP READ");
            }
            uint64_t count = 0;
            parse_u64("COUNT", count);
            if (!matchContextual("BLOCK_MS")) {
                errorCode("PRS_0504", "Expected BLOCK_MS in REDIS STREAM GROUP READ");
            }
            uint64_t block_ms = 0;
            parse_u64("BLOCK_MS", block_ms);
            return make_alter_system(
                "redis.stream.group.read",
                "REDIS STREAM GROUP READ STREAM " + stream_name +
                    " GROUP " + group_name +
                    " CONSUMER " + consumer_name +
                    " COUNT " + std::to_string(count) +
                    " BLOCK_MS " + std::to_string(block_ms));
        }

        if (matchContextual("CLAIM")) {
            expectContextual("STREAM", "Expected STREAM in REDIS STREAM GROUP CLAIM");
            std::string stream_name = parse_scalar_text("STREAM");
            expect_group_keyword("Expected GROUP in REDIS STREAM GROUP CLAIM");
            std::string group_name = parse_scalar_text("GROUP");
            expectContextual("CONSUMER", "Expected CONSUMER in REDIS STREAM GROUP CLAIM");
            std::string consumer_name = parse_scalar_text("CONSUMER");
            if (!matchContextual("MIN_IDLE_MS")) {
                errorCode("PRS_0504", "Expected MIN_IDLE_MS in REDIS STREAM GROUP CLAIM");
            }
            uint64_t min_idle_ms = 0;
            parse_u64("MIN_IDLE_MS", min_idle_ms);
            if (!matchContextual("IDS")) {
                errorCode("PRS_0504", "Expected IDS in REDIS STREAM GROUP CLAIM");
            }
            std::vector<std::string> ids = parse_ids();
            return make_alter_system(
                "redis.stream.group.claim",
                "REDIS STREAM GROUP CLAIM STREAM " + stream_name +
                    " GROUP " + group_name +
                    " CONSUMER " + consumer_name +
                    " MIN_IDLE_MS " + std::to_string(min_idle_ms) +
                    " IDS (" + join_csv(ids) + ")");
        }

        errorCode("PRS_0505", "Unsupported REDIS STREAM GROUP surface");
        auto* fallback = arena_.create<AlterSystemStmt>();
        fallback->span = makeSpan(start);
        return fallback;
    }

    errorCode("PRS_0505", "Unsupported REDIS stream group surface");
    auto* fallback = arena_.create<AlterSystemStmt>();
    fallback->span = makeSpan(start);
    return fallback;
}

Statement* Parser::parseUdrCompileSurface() {
    SourceLocation start = currentLocation();
    bool prefixed_by_udr = false;
    bool validate_only = false;

    auto parse_scalar = [&](const char* label) -> StringPool::StringId {
        if (check(TokenType::STRING_LITERAL) || isIdentifier()) {
            StringPool::StringId id = current().value.string_id;
            advance();
            return id;
        }
        if (check(TokenType::INTEGER_LITERAL)) {
            StringPool::StringId id = stringPool().intern(std::to_string(current().value.int_value));
            advance();
            return id;
        }
        if (check(TokenType::FLOAT_LITERAL)) {
            StringPool::StringId id = stringPool().intern(std::to_string(current().value.float_value));
            advance();
            return id;
        }
        errorCode("PRS_0504", std::string("Expected scalar token for ").append(label));
        return StringPool::INVALID_ID;
    };

    auto match_compile_or_validate = [&]() -> bool {
        if (matchContextual("COMPILE")) {
            validate_only = false;
            return true;
        }
        if (matchContextual("VALIDATE")) {
            validate_only = true;
            return true;
        }
        return false;
    };

    if (matchContextual("UDR")) {
        prefixed_by_udr = true;
        if (!match_compile_or_validate()) {
            errorCode("PRS_0505", "Expected COMPILE or VALIDATE after UDR");
        }
    } else if (!match_compile_or_validate()) {
        errorCode("PRS_0505", "Expected UDR compile or validate command surface");
    }

    if (matchContextual("EMBEDDED")) {
        if (!requireFeature(kFeatureLanguageUdrCompileBridge)) {
            auto* blocked = arena_.create<AlterSystemStmt>();
            blocked->span = makeSpan(start);
            return blocked;
        }
        if (!matchContextual("PAYLOAD")) {
            errorCode("PRS_0505", "Expected PAYLOAD after EMBEDDED");
        }
        auto* stmt = arena_.create<UdrCompileDispatchStmt>();
        stmt->validate_only = validate_only;

        if (prefixed_by_udr) {
            if (!matchContextual("PROFILE")) {
                errorCode("PRS_0504", "Expected PROFILE in UDR EMBEDDED PAYLOAD statement form");
            }
            stmt->profile_id = parse_scalar("PROFILE");
            if (!matchContextual("FORMAT")) {
                errorCode("PRS_0504", "Expected FORMAT in UDR EMBEDDED PAYLOAD statement form");
            }
            stmt->payload_format = parse_scalar("FORMAT");
            if (!matchContextual("BYTES")) {
                errorCode("PRS_0504", "Expected BYTES in UDR EMBEDDED PAYLOAD statement form");
            }
            stmt->payload_bytes = parse_scalar("BYTES");
            if (!matchContextual("SESSION_SIGNATURE")) {
                errorCode("PRS_0504", "Expected SESSION_SIGNATURE in UDR EMBEDDED PAYLOAD statement form");
            }
            stmt->session_signature = parse_scalar("SESSION_SIGNATURE");
        } else {
            stmt->profile_id = parse_scalar("profile_id");
            if (!match(TokenType::COMMA)) {
                errorCode("PRS_0504", "Expected ',' after profile_id in EMBEDDED PAYLOAD clause form");
            }
            stmt->payload_format = parse_scalar("payload_format");
            if (!match(TokenType::COMMA)) {
                errorCode("PRS_0504", "Expected ',' after payload_format in EMBEDDED PAYLOAD clause form");
            }
            stmt->payload_bytes = parse_scalar("payload_bytes");
            if (!match(TokenType::COMMA)) {
                errorCode("PRS_0504", "Expected ',' after payload_bytes in EMBEDDED PAYLOAD clause form");
            }
            stmt->session_signature = parse_scalar("session_signature");
        }

        stmt->span = makeSpan(start);
        return stmt;
    }

    if (matchContextual("SQL")) {
        if (!requireFeature(kFeatureEmbeddedSqlTemplateCompile)) {
            auto* blocked = arena_.create<AlterSystemStmt>();
            blocked->span = makeSpan(start);
            return blocked;
        }
        return parseUdrEmbeddedSqlTemplateSurface(validate_only, prefixed_by_udr);
    }

    errorCode("PRS_0505", "Unsupported UDR compile/validate surface");
    auto* fallback = arena_.create<AlterSystemStmt>();
    fallback->span = makeSpan(start);
    return fallback;
}

Statement* Parser::parseSelectUdrCompileFunctionSurface() {
    // SELECT COMPILE_EMBEDDED_PAYLOAD(...) and
    // SELECT VALIDATE_EMBEDDED_PAYLOAD(...) are canonical function-form
    // aliases for the UDR compile bridge dispatch route.
    SourceLocation start = previous().span.start;
    bool validate_only = false;

    auto parse_scalar = [&](const char* label) -> StringPool::StringId {
        if (check(TokenType::STRING_LITERAL) || isIdentifier()) {
            StringPool::StringId id = current().value.string_id;
            advance();
            return id;
        }
        if (check(TokenType::INTEGER_LITERAL)) {
            StringPool::StringId id = stringPool().intern(std::to_string(current().value.int_value));
            advance();
            return id;
        }
        if (check(TokenType::FLOAT_LITERAL)) {
            StringPool::StringId id = stringPool().intern(std::to_string(current().value.float_value));
            advance();
            return id;
        }
        errorCode("PRS_0504", std::string("Expected scalar token for ").append(label));
        return StringPool::INVALID_ID;
    };

    if (matchContextual("COMPILE_EMBEDDED_PAYLOAD")) {
        validate_only = false;
    } else if (matchContextual("VALIDATE_EMBEDDED_PAYLOAD")) {
        validate_only = true;
    } else {
        // Caller only dispatches here for function-form UDR compile symbols.
        // Fall back to regular SELECT parsing if the symbol no longer matches.
        return parseSelect();
    }

    if (!requireFeature(kFeatureLanguageUdrCompileBridge)) {
        auto* blocked = arena_.create<AlterSystemStmt>();
        blocked->span = makeSpan(start);
        return blocked;
    }

    expect(TokenType::LEFT_PAREN, "Expected '(' after UDR compile function symbol");

    auto* stmt = arena_.create<UdrCompileDispatchStmt>();
    stmt->validate_only = validate_only;

    stmt->profile_id = parse_scalar("profile_id");
    if (!match(TokenType::COMMA)) {
        errorCode("PRS_0504", "Expected ',' after profile_id in UDR compile function form");
    }
    stmt->payload_format = parse_scalar("payload_format");
    if (!match(TokenType::COMMA)) {
        errorCode("PRS_0504", "Expected ',' after payload_format in UDR compile function form");
    }
    stmt->payload_bytes = parse_scalar("payload_bytes");
    if (!match(TokenType::COMMA)) {
        errorCode("PRS_0504", "Expected ',' after payload_bytes in UDR compile function form");
    }
    stmt->session_signature = parse_scalar("session_signature");
    expect(TokenType::RIGHT_PAREN, "Expected ')' after UDR compile function arguments");

    // This function-form route is statement-oriented; extra trailing query
    // clauses are rejected deterministically so runtime dispatch remains
    // equivalent to canonical UDR COMPILE/VALIDATE statement surfaces.
    if (!check(TokenType::SEMICOLON) && !isAtEnd()) {
        errorCode("PRS_0505",
                  "Unsupported trailing clause for UDR compile function statement form");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseUdrEmbeddedSqlTemplateSurface(bool validate_only, bool prefixed_by_udr) {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<UdrEmbeddedSqlCompileStmt>();
    stmt->validate_only = validate_only;

    auto parse_scalar = [&](const char* label) -> StringPool::StringId {
        if (check(TokenType::STRING_LITERAL) || isIdentifier()) {
            StringPool::StringId id = current().value.string_id;
            advance();
            return id;
        }
        if (check(TokenType::INTEGER_LITERAL)) {
            StringPool::StringId id = stringPool().intern(std::to_string(current().value.int_value));
            advance();
            return id;
        }
        if (check(TokenType::FLOAT_LITERAL)) {
            StringPool::StringId id = stringPool().intern(std::to_string(current().value.float_value));
            advance();
            return id;
        }
        errorCode("PRS_0504", std::string("Expected scalar token for ").append(label));
        return StringPool::INVALID_ID;
    };

    if (!matchContextual("TEMPLATE")) {
        errorCode("PRS_0505", "Expected TEMPLATE after SQL in UDR compile/validate surface");
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (prefixed_by_udr) {
        if (!matchContextual("TEMPLATE_ID")) {
            errorCode("PRS_0504", "Expected TEMPLATE_ID in UDR SQL TEMPLATE statement form");
        }
        stmt->template_id = parse_scalar("TEMPLATE_ID");
        if (!matchContextual("SQL_TEXT")) {
            errorCode("PRS_0504", "Expected SQL_TEXT in UDR SQL TEMPLATE statement form");
        }
        stmt->sql_text = parse_scalar("SQL_TEXT");
        if (!matchContextual("PROFILE")) {
            errorCode("PRS_0504", "Expected PROFILE in UDR SQL TEMPLATE statement form");
        }
        stmt->profile_id = parse_scalar("PROFILE");
        if (!matchContextual("SESSION_SIGNATURE")) {
            errorCode("PRS_0504", "Expected SESSION_SIGNATURE in UDR SQL TEMPLATE statement form");
        }
        stmt->session_signature = parse_scalar("SESSION_SIGNATURE");
    } else {
        stmt->template_id = parse_scalar("template_id");
        if (!match(TokenType::KW_USING) && !matchContextual("USING")) {
            errorCode("PRS_0504", "Expected USING in SQL TEMPLATE clause form");
        }
        stmt->sql_text = parse_scalar("sql_text");
        if (!matchContextual("PROFILE")) {
            errorCode("PRS_0504", "Expected PROFILE in SQL TEMPLATE clause form");
        }
        stmt->profile_id = parse_scalar("PROFILE");
        if (!(matchContextual("SIGNATURE") || matchContextual("SESSION_SIGNATURE"))) {
            errorCode("PRS_0504", "Expected SIGNATURE in SQL TEMPLATE clause form");
        }
        stmt->session_signature = parse_scalar("SIGNATURE");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseInstallExtensionSurface(bool is_load_surface) {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<AlterSystemStmt>();

    bool if_not_exists = false;
    if (match(TokenType::KW_IF)) {
        expect(TokenType::KW_NOT, "Expected NOT after IF");
        expect(TokenType::KW_EXISTS, "Expected EXISTS after IF NOT");
        if_not_exists = true;
    }

    // Support both canonical and short forms:
    // INSTALL EXTENSION ext / INSTALL ext
    // LOAD EXTENSION ext / LOAD ext
    matchContextual("EXTENSION");

    StringPool::StringId extension_name =
        expectIdentifier("Expected extension name in INSTALL/LOAD EXTENSION");
    std::string extension = std::string(stringPool().get(extension_name));
    std::string payload = captureStatementBody();
    if (if_not_exists) {
        if (!payload.empty()) {
            payload.insert(0, ";");
        }
        payload.insert(0, "IF_NOT_EXISTS=1");
    }

    stmt->name =
        stringPool().intern(std::string("platform.extension.") +
                            (is_load_surface ? "load." : "install.") + extension);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern(payload);
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// DCL Statement Parsing (GRANT/REVOKE)
// =============================================================================

Statement* Parser::parseRevokeToken() {
    SourceLocation start = currentLocation();

    if (!matchContextual("TOKEN")) {
        errorCode("PRS_0505", "Expected TOKEN after REVOKE");
        return nullptr;
    }

    auto* stmt = arena_.create<AlterSystemStmt>();
    StringPool::StringId token_name = expectIdentifier("Expected token name");
    std::string token = std::string(stringPool().get(token_name));
    stmt->name = stringPool().intern("security.token.revoke." + token);
    auto* lit = arena_.create<LiteralExpr>();
    lit->literal_type = LiteralType::STRING;
    lit->string_value = stringPool().intern("");
    stmt->value = lit;
    stmt->span = makeSpan(start);
    return stmt;
}

GrantStmt* Parser::parseGrant() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<GrantStmt>();

    // Parse privileges
    do {
        if (match(TokenType::KW_SELECT) || matchContextual("SELECT")) {
            stmt->privileges.push_back(PrivilegeType::SELECT);
        } else if (match(TokenType::KW_INSERT)) {
            stmt->privileges.push_back(PrivilegeType::INSERT);
        } else if (match(TokenType::KW_UPDATE)) {
            stmt->privileges.push_back(PrivilegeType::UPDATE);
        } else if (match(TokenType::KW_DELETE)) {
            stmt->privileges.push_back(PrivilegeType::DELETE);
        } else if (matchContextual("TRUNCATE")) {
            stmt->privileges.push_back(PrivilegeType::TRUNCATE);
        } else if (matchContextual("REFERENCES")) {
            stmt->privileges.push_back(PrivilegeType::REFERENCES);
        } else if (matchContextual("TRIGGER")) {
            stmt->privileges.push_back(PrivilegeType::TRIGGER);
        } else if (match(TokenType::KW_EXECUTE) || matchContextual("EXECUTE")) {
            if (matchContextual("EXTERNAL")) {
                expectContextual("JOB", "Expected JOB after EXECUTE EXTERNAL");
                stmt->privileges.push_back(PrivilegeType::EXECUTE_EXTERNAL_JOB);
            } else {
                stmt->privileges.push_back(PrivilegeType::EXECUTE);
            }
        } else if (matchContextual("USAGE")) {
            stmt->privileges.push_back(PrivilegeType::USAGE);
        } else if (match(TokenType::KW_COPY)) {
            stmt->privileges.push_back(PrivilegeType::COPY);
        } else if (matchContextual("CREATE")) {
            if (checkContextual("JOB")) {
                expectContextual("JOB", "Expected JOB after CREATE");
                stmt->privileges.push_back(PrivilegeType::CREATE_JOB);
            } else {
                stmt->privileges.push_back(PrivilegeType::CREATE);
            }
        } else if (matchContextual("CONNECT")) {
            stmt->privileges.push_back(PrivilegeType::CONNECT);
        } else if (matchContextual("TEMPORARY")) {
            stmt->privileges.push_back(PrivilegeType::TEMPORARY);
        } else if (matchContextual("VIEW")) {
            expectContextual("JOB", "Expected JOB after VIEW");
            if (!matchContextual("HISTORY")) {
                error("Expected HISTORY after VIEW JOB");
                return nullptr;
            }
            stmt->privileges.push_back(PrivilegeType::VIEW_JOB_HISTORY);
        } else if (matchContextual("ALL")) {
            matchContextual("PRIVILEGES");  // Optional
            stmt->privileges.push_back(PrivilegeType::ALL);
        } else {
            error("Expected privilege type");
            return nullptr;
        }
    } while (match(TokenType::COMMA));

    // ON object_type
    expect(TokenType::KW_ON, "Expected ON");

    if (matchContextual("TABLE")) {
        stmt->object_type = PrivilegeObjectType::TABLE;
    } else if (matchContextual("JOB")) {
        stmt->object_type = PrivilegeObjectType::JOB;
    } else if (matchContextual("SEQUENCE")) {
        stmt->object_type = PrivilegeObjectType::SEQUENCE;
    } else if (matchContextual("FUNCTION")) {
        stmt->object_type = PrivilegeObjectType::FUNCTION;
    } else if (matchContextual("PROCEDURE")) {
        stmt->object_type = PrivilegeObjectType::PROCEDURE;
    } else if (matchContextual("SCHEMA")) {
        stmt->object_type = PrivilegeObjectType::SCHEMA;
    } else if (matchContextual("DATABASE")) {
        stmt->object_type = PrivilegeObjectType::DATABASE;
    } else {
        // Default to TABLE
        stmt->object_type = PrivilegeObjectType::TABLE;
    }

    // Parse object names
    do {
        stmt->objects.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    // TO grantees
    expectContextual("TO", "Expected TO");

    do {
        if (matchContextual("PUBLIC")) {
            stmt->is_public = true;
        } else {
            stmt->grantees.push_back(expectIdentifier("Expected grantee name"));
        }
    } while (match(TokenType::COMMA));

    // WITH GRANT OPTION
    if (match(TokenType::KW_WITH)) {
        expect(TokenType::KW_GRANT, "Expected GRANT after WITH");
        expectContextual("OPTION", "Expected OPTION after GRANT");
        stmt->with_grant_option = true;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

RevokeStmt* Parser::parseRevoke() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<RevokeStmt>();

    // REVOKE GRANT OPTION FOR?
    if (match(TokenType::KW_GRANT)) {
        expectContextual("OPTION", "Expected OPTION after GRANT");
        expectContextual("FOR", "Expected FOR after OPTION");
        stmt->grant_option_for = true;
    }

    // Parse privileges
    do {
        if (match(TokenType::KW_SELECT) || matchContextual("SELECT")) {
            stmt->privileges.push_back(PrivilegeType::SELECT);
        } else if (match(TokenType::KW_INSERT)) {
            stmt->privileges.push_back(PrivilegeType::INSERT);
        } else if (match(TokenType::KW_UPDATE)) {
            stmt->privileges.push_back(PrivilegeType::UPDATE);
        } else if (match(TokenType::KW_DELETE)) {
            stmt->privileges.push_back(PrivilegeType::DELETE);
        } else if (matchContextual("TRUNCATE")) {
            stmt->privileges.push_back(PrivilegeType::TRUNCATE);
        } else if (matchContextual("REFERENCES")) {
            stmt->privileges.push_back(PrivilegeType::REFERENCES);
        } else if (matchContextual("TRIGGER")) {
            stmt->privileges.push_back(PrivilegeType::TRIGGER);
        } else if (match(TokenType::KW_EXECUTE) || matchContextual("EXECUTE")) {
            if (matchContextual("EXTERNAL")) {
                expectContextual("JOB", "Expected JOB after EXECUTE EXTERNAL");
                stmt->privileges.push_back(PrivilegeType::EXECUTE_EXTERNAL_JOB);
            } else {
                stmt->privileges.push_back(PrivilegeType::EXECUTE);
            }
        } else if (matchContextual("USAGE")) {
            stmt->privileges.push_back(PrivilegeType::USAGE);
        } else if (match(TokenType::KW_COPY)) {
            stmt->privileges.push_back(PrivilegeType::COPY);
        } else if (matchContextual("CREATE")) {
            if (checkContextual("JOB")) {
                expectContextual("JOB", "Expected JOB after CREATE");
                stmt->privileges.push_back(PrivilegeType::CREATE_JOB);
            } else {
                stmt->privileges.push_back(PrivilegeType::CREATE);
            }
        } else if (matchContextual("CONNECT")) {
            stmt->privileges.push_back(PrivilegeType::CONNECT);
        } else if (matchContextual("TEMPORARY")) {
            stmt->privileges.push_back(PrivilegeType::TEMPORARY);
        } else if (matchContextual("VIEW")) {
            expectContextual("JOB", "Expected JOB after VIEW");
            if (!matchContextual("HISTORY")) {
                error("Expected HISTORY after VIEW JOB");
                return nullptr;
            }
            stmt->privileges.push_back(PrivilegeType::VIEW_JOB_HISTORY);
        } else if (matchContextual("ALL")) {
            matchContextual("PRIVILEGES");
            stmt->privileges.push_back(PrivilegeType::ALL);
        } else {
            error("Expected privilege type");
            return nullptr;
        }
    } while (match(TokenType::COMMA));

    // ON object_type
    expect(TokenType::KW_ON, "Expected ON");

    if (matchContextual("TABLE")) {
        stmt->object_type = PrivilegeObjectType::TABLE;
    } else if (matchContextual("JOB")) {
        stmt->object_type = PrivilegeObjectType::JOB;
    } else if (matchContextual("SEQUENCE")) {
        stmt->object_type = PrivilegeObjectType::SEQUENCE;
    } else if (matchContextual("FUNCTION")) {
        stmt->object_type = PrivilegeObjectType::FUNCTION;
    } else if (matchContextual("PROCEDURE")) {
        stmt->object_type = PrivilegeObjectType::PROCEDURE;
    } else if (matchContextual("SCHEMA")) {
        stmt->object_type = PrivilegeObjectType::SCHEMA;
    } else if (matchContextual("DATABASE")) {
        stmt->object_type = PrivilegeObjectType::DATABASE;
    } else {
        stmt->object_type = PrivilegeObjectType::TABLE;
    }

    // Parse object names
    do {
        stmt->objects.push_back(parseSchemaPath(state_));
    } while (match(TokenType::COMMA));

    // FROM grantees
    expect(TokenType::KW_FROM, "Expected FROM");

    do {
        if (matchContextual("PUBLIC")) {
            stmt->is_public = true;
        } else {
            stmt->grantees.push_back(expectIdentifier("Expected grantee name"));
        }
    } while (match(TokenType::COMMA));

    // CASCADE/RESTRICT
    if (matchContextual("CASCADE")) {
        stmt->cascade = true;
    } else {
        matchContextual("RESTRICT");  // Optional, default
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// Connection Statement Parsing
// =============================================================================

ConnectStmt* Parser::parseConnect() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<ConnectStmt>();

    // CONNECT [TO] database
    matchContextual("TO");  // Optional

    stmt->database = expectIdentifier("Expected database name");

    // Optional USER/PASSWORD/ROLE/CHARSET
    while (true) {
        if (matchContextual("USER")) {
            stmt->user = expectIdentifier("Expected user name");
        } else if (matchContextual("PASSWORD")) {
            if (check(TokenType::STRING_LITERAL)) {
                stmt->password = current().value.string_id;
                advance();
            } else {
                stmt->password = expectIdentifier("Expected password");
            }
        } else if (matchContextual("ROLE")) {
            stmt->role = expectIdentifier("Expected role name");
        } else if (matchContextual("CHARSET") || matchContextual("CHARACTER")) {
            if (checkContextual("SET")) {
                matchContextual("SET");
            }
            stmt->charset = expectIdentifier("Expected charset name");
        } else {
            break;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

DisconnectStmt* Parser::parseDisconnect() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<DisconnectStmt>();

    if (matchContextual("ALL")) {
        stmt->target = DisconnectStmt::Target::ALL;
    } else if (matchContextual("CURRENT")) {
        stmt->target = DisconnectStmt::Target::CURRENT;
    } else if (!isAtEnd() && !check(TokenType::SEMICOLON)) {
        stmt->target = DisconnectStmt::Target::NAMED;
        stmt->connection_name = expectIdentifier("Expected connection name");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// Metadata Statement Parsing (COMMENT)
// =============================================================================

CommentStmt* Parser::parseComment() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<CommentStmt>();

    expect(TokenType::KW_ON, "Expected ON after COMMENT");

    auto parseObjectType = [&](CommentObjectType& out_type) -> bool {
        if (matchContextual("TABLE")) {
            out_type = CommentObjectType::TABLE;
            return true;
        }
        if (matchContextual("COLUMN")) {
            out_type = CommentObjectType::COLUMN;
            return true;
        }
        if (matchContextual("INDEX")) {
            out_type = CommentObjectType::INDEX;
            return true;
        }
        if (matchContextual("VIEW")) {
            out_type = CommentObjectType::VIEW;
            return true;
        }
        if (matchContextual("SEQUENCE")) {
            out_type = CommentObjectType::SEQUENCE;
            return true;
        }
        if (matchContextual("FUNCTION")) {
            out_type = CommentObjectType::FUNCTION;
            return true;
        }
        if (matchContextual("PROCEDURE")) {
            out_type = CommentObjectType::PROCEDURE;
            return true;
        }
        if (matchContextual("TRIGGER")) {
            out_type = CommentObjectType::TRIGGER;
            return true;
        }
        if (matchContextual("SCHEMA")) {
            out_type = CommentObjectType::SCHEMA;
            return true;
        }
        if (matchContextual("DATABASE")) {
            out_type = CommentObjectType::DATABASE;
            return true;
        }
        if (matchContextual("ROLE")) {
            out_type = CommentObjectType::ROLE;
            return true;
        }
        if (matchContextual("CONSTRAINT")) {
            out_type = CommentObjectType::CONSTRAINT;
            return true;
        }
        if (matchContextual("DOMAIN")) {
            out_type = CommentObjectType::DOMAIN;
            return true;
        }
        if (matchContextual("TYPE")) {
            out_type = CommentObjectType::TYPE;
            return true;
        }
        if (matchContextual("PACKAGE")) {
            out_type = CommentObjectType::PACKAGE;
            return true;
        }
        if (matchContextual("EXCEPTION")) {
            out_type = CommentObjectType::EXCEPTION;
            return true;
        }
        if (matchContextual("UDR")) {
            out_type = CommentObjectType::UDR;
            return true;
        }
        if (matchContextual("USER")) {
            if (matchContextual("MAPPING")) {
                out_type = CommentObjectType::USER_MAPPING;
            } else {
                out_type = CommentObjectType::USER;
            }
            return true;
        }
        if (matchContextual("GROUP")) {
            out_type = CommentObjectType::GROUP;
            return true;
        }
        if (matchContextual("POLICY")) {
            out_type = CommentObjectType::POLICY;
            return true;
        }
        if (matchContextual("TOKEN")) {
            out_type = CommentObjectType::TOKEN;
            return true;
        }
        if (matchContextual("QUOTA")) {
            expectContextual("PROFILE", "Expected PROFILE after QUOTA");
            out_type = CommentObjectType::QUOTA_PROFILE;
            return true;
        }
        if (matchContextual("CONNECTION")) {
            expectContextual("RULE", "Expected RULE after CONNECTION");
            out_type = CommentObjectType::CONNECTION_RULE;
            return true;
        }
        if (matchContextual("SERVER")) {
            out_type = CommentObjectType::SERVER;
            return true;
        }
        if (matchContextual("FOREIGN")) {
            if (matchContextual("DATA")) {
                expectContextual("WRAPPER", "Expected WRAPPER after FOREIGN DATA");
                out_type = CommentObjectType::FOREIGN_DATA_WRAPPER;
                return true;
            }
            if (matchContextual("TABLE")) {
                out_type = CommentObjectType::FOREIGN_TABLE;
                return true;
            }
            error("Expected TABLE or DATA WRAPPER after FOREIGN");
            return false;
        }
        if (matchContextual("SYNONYM")) {
            out_type = CommentObjectType::SYNONYM;
            return true;
        }
        if (matchContextual("PUBLIC")) {
            if (matchContextual("SYNONYM")) {
                out_type = CommentObjectType::PUBLIC_SYNONYM;
                return true;
            }
            if (matchContextual("PUBLICATION")) {
                out_type = CommentObjectType::PUBLICATION;
                return true;
            }
            error("Expected SYNONYM or PUBLICATION after PUBLIC");
            return false;
        }
        if (matchContextual("PUBLICATION")) {
            out_type = CommentObjectType::PUBLICATION;
            return true;
        }
        if (matchContextual("SUBSCRIPTION")) {
            out_type = CommentObjectType::SUBSCRIPTION;
            return true;
        }
        if (matchContextual("REPLICATION")) {
            expectContextual("CHANNEL", "Expected CHANNEL after REPLICATION");
            out_type = CommentObjectType::REPLICATION_CHANNEL;
            return true;
        }
        if (matchContextual("JOB")) {
            out_type = CommentObjectType::JOB;
            return true;
        }
        if (matchContextual("EVENT")) {
            out_type = CommentObjectType::EVENT;
            return true;
        }
        if (matchContextual("TABLESPACE")) {
            out_type = CommentObjectType::TABLESPACE;
            return true;
        }
        if (matchContextual("FILESPACE")) {
            out_type = CommentObjectType::FILESPACE;
            return true;
        }
        if (matchContextual("CLUSTER")) {
            out_type = CommentObjectType::CLUSTER;
            return true;
        }
        if (matchContextual("CUBE")) {
            out_type = CommentObjectType::CUBE;
            return true;
        }
        return false;
    };

    auto skipSignatureList = [&]() {
        if (!match(TokenType::LEFT_PAREN)) {
            return;
        }
        int depth = 1;
        while (!isAtEnd() && depth > 0) {
            if (match(TokenType::LEFT_PAREN)) {
                depth++;
            } else if (match(TokenType::RIGHT_PAREN)) {
                depth--;
            } else {
                advance();
            }
        }
    };

    auto enforceParentRule = [&](CommentObjectType type, const SchemaPath& path) {
        if (type == CommentObjectType::COLUMN ||
            type == CommentObjectType::CONSTRAINT ||
            type == CommentObjectType::INDEX) {
            if (path.components.size() < 2) {
                error("Parent path is required for COMMENT ON COLUMN/CONSTRAINT/INDEX");
            }
        }
    };

    // Compatibility grammar:
    // COMMENT ON <object_type> <path> IS ...
    if (parseObjectType(stmt->object_type)) {
        if (stmt->object_type == CommentObjectType::USER_MAPPING) {
            expectContextual("FOR", "Expected FOR after COMMENT ON USER MAPPING");

            StringPool::StringId principal = StringPool::INVALID_ID;
            if (matchContextual("CURRENT_USER")) {
                principal = stringPool().intern("CURRENT_USER");
            } else if (matchContextual("SESSION_USER")) {
                principal = stringPool().intern("SESSION_USER");
            } else if (matchContextual("PUBLIC")) {
                principal = stringPool().intern("PUBLIC");
            } else if (matchContextual("USER")) {
                if (isIdentifier()) {
                    principal = expectIdentifier("Expected user name");
                } else {
                    principal = stringPool().intern("CURRENT_USER");
                }
            } else if (isIdentifier()) {
                principal = expectIdentifier("Expected principal name");
            } else {
                error("Expected principal after COMMENT ON USER MAPPING FOR");
            }

            expectContextual("SERVER", "Expected SERVER after USER MAPPING principal");
            stmt->object_path = parseSchemaPath(state_);
            if (principal != StringPool::INVALID_ID) {
                stmt->object_path.components.push_back(principal);
            }
        } else {
            stmt->object_path = parseSchemaPath(state_);

            if (stmt->object_type == CommentObjectType::FUNCTION ||
                stmt->object_type == CommentObjectType::PROCEDURE) {
                skipSignatureList();
            } else if (stmt->object_type == CommentObjectType::TRIGGER ||
                       stmt->object_type == CommentObjectType::CONSTRAINT ||
                       stmt->object_type == CommentObjectType::POLICY) {
                if (matchContextual("ON")) {
                    SchemaPath parent = parseSchemaPath(state_);
                    if (!stmt->object_path.components.empty()) {
                        SchemaPath qualified = parent;
                        qualified.components.push_back(stmt->object_path.components.back());
                        stmt->object_path = std::move(qualified);
                    }
                }
            }
        }
    } else {
        // Canonical grammar:
        // COMMENT ON <object_name> OF <object_type> IN <path> IS <text|NULL>
        StringPool::StringId object_name = expectIdentifier("Expected object name after COMMENT ON");
        expectContextual("OF", "Expected OF after object name in COMMENT ON");
        if (!parseObjectType(stmt->object_type)) {
            error("Expected object type after COMMENT ON <name> OF");
            return nullptr;
        }
        if (!(match(TokenType::KW_IN) || match(TokenType::KW_FROM))) {
            error("Expected IN/FROM after COMMENT ON <name> OF <type>");
            return nullptr;
        }
        SchemaPath parent_path = parseSchemaPath(state_);
        stmt->object_path = parent_path;
        stmt->object_path.components.push_back(object_name);
        if (stmt->object_type == CommentObjectType::COLUMN) {
            stmt->column_name = object_name;
        }
    }

    enforceParentRule(stmt->object_type, stmt->object_path);

    // IS 'comment' or IS NULL
    expect(TokenType::KW_IS, "Expected IS");

    if (match(TokenType::KW_NULL)) {
        stmt->is_null = true;
        stmt->action = CommentStmt::Action::SET_NULL;
    } else if (check(TokenType::STRING_LITERAL)) {
        stmt->comment_text = current().value.string_id;
        advance();
        stmt->action = CommentStmt::Action::SET;
    } else {
        error("Expected string literal or NULL after IS");
        return nullptr;
    }

    stmt->span = makeSpan(start);
    return stmt;
}

CommentStmt* Parser::parseDropComment() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<CommentStmt>();

    expect(TokenType::KW_ON, "Expected ON after DROP COMMENT");

    auto parseObjectType = [&](CommentObjectType& out_type) -> bool {
        if (matchContextual("TABLE")) {
            out_type = CommentObjectType::TABLE;
            return true;
        }
        if (matchContextual("COLUMN")) {
            out_type = CommentObjectType::COLUMN;
            return true;
        }
        if (matchContextual("INDEX")) {
            out_type = CommentObjectType::INDEX;
            return true;
        }
        if (matchContextual("VIEW")) {
            out_type = CommentObjectType::VIEW;
            return true;
        }
        if (matchContextual("SEQUENCE")) {
            out_type = CommentObjectType::SEQUENCE;
            return true;
        }
        if (matchContextual("FUNCTION")) {
            out_type = CommentObjectType::FUNCTION;
            return true;
        }
        if (matchContextual("PROCEDURE")) {
            out_type = CommentObjectType::PROCEDURE;
            return true;
        }
        if (matchContextual("TRIGGER")) {
            out_type = CommentObjectType::TRIGGER;
            return true;
        }
        if (matchContextual("SCHEMA")) {
            out_type = CommentObjectType::SCHEMA;
            return true;
        }
        if (matchContextual("DATABASE")) {
            out_type = CommentObjectType::DATABASE;
            return true;
        }
        if (matchContextual("ROLE")) {
            out_type = CommentObjectType::ROLE;
            return true;
        }
        if (matchContextual("CONSTRAINT")) {
            out_type = CommentObjectType::CONSTRAINT;
            return true;
        }
        if (matchContextual("DOMAIN")) {
            out_type = CommentObjectType::DOMAIN;
            return true;
        }
        if (matchContextual("TYPE")) {
            out_type = CommentObjectType::TYPE;
            return true;
        }
        if (matchContextual("PACKAGE")) {
            out_type = CommentObjectType::PACKAGE;
            return true;
        }
        if (matchContextual("EXCEPTION")) {
            out_type = CommentObjectType::EXCEPTION;
            return true;
        }
        if (matchContextual("UDR")) {
            out_type = CommentObjectType::UDR;
            return true;
        }
        if (matchContextual("USER")) {
            if (matchContextual("MAPPING")) {
                out_type = CommentObjectType::USER_MAPPING;
            } else {
                out_type = CommentObjectType::USER;
            }
            return true;
        }
        if (matchContextual("GROUP")) {
            out_type = CommentObjectType::GROUP;
            return true;
        }
        if (matchContextual("POLICY")) {
            out_type = CommentObjectType::POLICY;
            return true;
        }
        if (matchContextual("TOKEN")) {
            out_type = CommentObjectType::TOKEN;
            return true;
        }
        if (matchContextual("QUOTA")) {
            expectContextual("PROFILE", "Expected PROFILE after QUOTA");
            out_type = CommentObjectType::QUOTA_PROFILE;
            return true;
        }
        if (matchContextual("CONNECTION")) {
            expectContextual("RULE", "Expected RULE after CONNECTION");
            out_type = CommentObjectType::CONNECTION_RULE;
            return true;
        }
        if (matchContextual("SERVER")) {
            out_type = CommentObjectType::SERVER;
            return true;
        }
        if (matchContextual("FOREIGN")) {
            if (matchContextual("DATA")) {
                expectContextual("WRAPPER", "Expected WRAPPER after FOREIGN DATA");
                out_type = CommentObjectType::FOREIGN_DATA_WRAPPER;
                return true;
            }
            if (matchContextual("TABLE")) {
                out_type = CommentObjectType::FOREIGN_TABLE;
                return true;
            }
            error("Expected TABLE or DATA WRAPPER after FOREIGN");
            return false;
        }
        if (matchContextual("SYNONYM")) {
            out_type = CommentObjectType::SYNONYM;
            return true;
        }
        if (matchContextual("PUBLIC")) {
            if (matchContextual("SYNONYM")) {
                out_type = CommentObjectType::PUBLIC_SYNONYM;
                return true;
            }
            if (matchContextual("PUBLICATION")) {
                out_type = CommentObjectType::PUBLICATION;
                return true;
            }
            error("Expected SYNONYM or PUBLICATION after PUBLIC");
            return false;
        }
        if (matchContextual("PUBLICATION")) {
            out_type = CommentObjectType::PUBLICATION;
            return true;
        }
        if (matchContextual("SUBSCRIPTION")) {
            out_type = CommentObjectType::SUBSCRIPTION;
            return true;
        }
        if (matchContextual("REPLICATION")) {
            expectContextual("CHANNEL", "Expected CHANNEL after REPLICATION");
            out_type = CommentObjectType::REPLICATION_CHANNEL;
            return true;
        }
        if (matchContextual("JOB")) {
            out_type = CommentObjectType::JOB;
            return true;
        }
        if (matchContextual("EVENT")) {
            out_type = CommentObjectType::EVENT;
            return true;
        }
        if (matchContextual("TABLESPACE")) {
            out_type = CommentObjectType::TABLESPACE;
            return true;
        }
        if (matchContextual("FILESPACE")) {
            out_type = CommentObjectType::FILESPACE;
            return true;
        }
        if (matchContextual("CLUSTER")) {
            out_type = CommentObjectType::CLUSTER;
            return true;
        }
        if (matchContextual("CUBE")) {
            out_type = CommentObjectType::CUBE;
            return true;
        }
        return false;
    };

    auto skipSignatureList = [&]() {
        if (!match(TokenType::LEFT_PAREN)) {
            return;
        }
        int depth = 1;
        while (!isAtEnd() && depth > 0) {
            if (match(TokenType::LEFT_PAREN)) {
                depth++;
            } else if (match(TokenType::RIGHT_PAREN)) {
                depth--;
            } else {
                advance();
            }
        }
    };

    auto enforceParentRule = [&](CommentObjectType type, const SchemaPath& path) {
        if (type == CommentObjectType::COLUMN ||
            type == CommentObjectType::CONSTRAINT ||
            type == CommentObjectType::INDEX) {
            if (path.components.size() < 2) {
                error("Parent path is required for DROP COMMENT ON COLUMN/CONSTRAINT/INDEX");
            }
        }
    };

    if (parseObjectType(stmt->object_type)) {
        if (stmt->object_type == CommentObjectType::USER_MAPPING) {
            expectContextual("FOR", "Expected FOR after DROP COMMENT ON USER MAPPING");

            StringPool::StringId principal = StringPool::INVALID_ID;
            if (matchContextual("CURRENT_USER")) {
                principal = stringPool().intern("CURRENT_USER");
            } else if (matchContextual("SESSION_USER")) {
                principal = stringPool().intern("SESSION_USER");
            } else if (matchContextual("PUBLIC")) {
                principal = stringPool().intern("PUBLIC");
            } else if (matchContextual("USER")) {
                if (isIdentifier()) {
                    principal = expectIdentifier("Expected user name");
                } else {
                    principal = stringPool().intern("CURRENT_USER");
                }
            } else if (isIdentifier()) {
                principal = expectIdentifier("Expected principal name");
            } else {
                error("Expected principal after DROP COMMENT ON USER MAPPING FOR");
            }

            expectContextual("SERVER", "Expected SERVER after USER MAPPING principal");
            stmt->object_path = parseSchemaPath(state_);
            if (principal != StringPool::INVALID_ID) {
                stmt->object_path.components.push_back(principal);
            }
        } else {
            stmt->object_path = parseSchemaPath(state_);

            if (stmt->object_type == CommentObjectType::FUNCTION ||
                stmt->object_type == CommentObjectType::PROCEDURE) {
                skipSignatureList();
            } else if (stmt->object_type == CommentObjectType::TRIGGER ||
                       stmt->object_type == CommentObjectType::CONSTRAINT ||
                       stmt->object_type == CommentObjectType::POLICY) {
                if (matchContextual("ON")) {
                    SchemaPath parent = parseSchemaPath(state_);
                    if (!stmt->object_path.components.empty()) {
                        SchemaPath qualified = parent;
                        qualified.components.push_back(stmt->object_path.components.back());
                        stmt->object_path = std::move(qualified);
                    }
                }
            }
        }
    } else {
        StringPool::StringId object_name = expectIdentifier("Expected object name after DROP COMMENT ON");
        expectContextual("OF", "Expected OF after object name");
        if (!parseObjectType(stmt->object_type)) {
            error("Expected object type after DROP COMMENT ON <name> OF");
            return nullptr;
        }
        if (!(match(TokenType::KW_IN) || match(TokenType::KW_FROM))) {
            error("Expected IN/FROM after DROP COMMENT ON <name> OF <type>");
            return nullptr;
        }

        SchemaPath parent_path = parseSchemaPath(state_);
        stmt->object_path = parent_path;
        stmt->object_path.components.push_back(object_name);
        if (stmt->object_type == CommentObjectType::COLUMN) {
            stmt->column_name = object_name;
        }
    }

    enforceParentRule(stmt->object_type, stmt->object_path);

    stmt->is_null = true;
    stmt->action = CommentStmt::Action::DROP;
    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// MERGE Statement Parsing
// =============================================================================

MergeStmt* Parser::parseMerge() {
    SourceLocation start = currentLocation();
    auto* stmt = arena_.create<MergeStmt>();

    // MERGE INTO target_table [AS alias]
    expect(TokenType::KW_INTO, "Expected INTO after MERGE");
    stmt->target_table = parseSchemaPath(state_);

    if (match(TokenType::KW_AS) || (check(TokenType::IDENTIFIER) && !checkContextual("USING"))) {
        stmt->target_alias = expectIdentifier("Expected target alias");
    }

    // USING source
    if (!(match(TokenType::KW_USING) || matchContextual("USING"))) {
        error("Expected USING");
        return nullptr;
    }

    if (match(TokenType::LEFT_PAREN)) {
        // Subquery
        if (!match(TokenType::KW_SELECT)) {
            error("Expected SELECT in MERGE USING subquery");
            return nullptr;
        }
        stmt->source_query = parseSelect();
        expect(TokenType::RIGHT_PAREN, "Expected ) after subquery");
    } else {
        stmt->source_table = parseSchemaPath(state_);
    }

    // Source alias
    if (match(TokenType::KW_AS) || (check(TokenType::IDENTIFIER) && !checkContextual("ON"))) {
        stmt->source_alias = expectIdentifier("Expected source alias");
    }

    // ON condition
    expect(TokenType::KW_ON, "Expected ON");
    stmt->on_condition = parseExpression();

    // WHEN clauses
    while (match(TokenType::KW_WHEN)) {
        if (matchContextual("MATCHED")) {
            MergeStmt::WhenMatched when;

            // AND condition
            if (match(TokenType::KW_AND)) {
                when.and_condition = parseExpression();
            }

            expect(TokenType::KW_THEN, "Expected THEN");

            if (match(TokenType::KW_UPDATE)) {
                expect(TokenType::KW_SET, "Expected SET after UPDATE");
                // Parse assignments
                do {
                    auto col = expectIdentifier("Expected column name");
                    expect(TokenType::EQUAL, "Expected = in assignment");
                    auto* expr = parseExpression();
                    when.assignments.emplace_back(col, expr);
                } while (match(TokenType::COMMA));
            } else if (match(TokenType::KW_DELETE)) {
                when.is_delete = true;
            } else {
                error("Expected UPDATE or DELETE after THEN");
                return nullptr;
            }

            stmt->when_matched.push_back(std::move(when));

        } else if (match(TokenType::KW_NOT)) {
            expectContextual("MATCHED", "Expected MATCHED after NOT");

            // Check for BY SOURCE (SQL Server extension)
            if (matchContextual("BY")) {
                expectContextual("SOURCE", "Expected SOURCE after BY");
                if (!requireFeature(kFeatureDmlMergeNotMatchedBySource)) {
                    stmt->span = makeSpan(start);
                    return stmt;
                }
                MergeStmt::WhenNotMatchedBySource when;

                if (match(TokenType::KW_AND)) {
                    when.and_condition = parseExpression();
                }

                expect(TokenType::KW_THEN, "Expected THEN");

                if (match(TokenType::KW_UPDATE)) {
                    expect(TokenType::KW_SET, "Expected SET after UPDATE");
                    do {
                        auto col = expectIdentifier("Expected column name");
                        expect(TokenType::EQUAL, "Expected = in assignment");
                        auto* expr = parseExpression();
                        when.assignments.emplace_back(col, expr);
                    } while (match(TokenType::COMMA));
                } else if (match(TokenType::KW_DELETE)) {
                    when.is_delete = true;
                }

                stmt->when_not_matched_by_source.push_back(std::move(when));

            } else {
                // WHEN NOT MATCHED [BY TARGET] THEN INSERT
                matchContextual("BY");
                matchContextual("TARGET");

                MergeStmt::WhenNotMatched when;

                if (match(TokenType::KW_AND)) {
                    when.and_condition = parseExpression();
                }

                expect(TokenType::KW_THEN, "Expected THEN");
                expect(TokenType::KW_INSERT, "Expected INSERT");

                // Optional column list
                if (match(TokenType::LEFT_PAREN)) {
                    do {
                        when.columns.push_back(expectIdentifier("Expected column name"));
                    } while (match(TokenType::COMMA));
                    expect(TokenType::RIGHT_PAREN, "Expected )");
                }

                // VALUES
                expect(TokenType::KW_VALUES, "Expected VALUES");
                expect(TokenType::LEFT_PAREN, "Expected (");
                do {
                    when.values.push_back(parseExpression());
                } while (match(TokenType::COMMA));
                expect(TokenType::RIGHT_PAREN, "Expected )");

                stmt->when_not_matched.push_back(std::move(when));
            }
        } else {
            error("Expected MATCHED or NOT MATCHED after WHEN");
            return nullptr;
        }
    }

    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// PSQL Statements (Procedural SQL)
// =============================================================================

Statement* Parser::parsePSQLStatement() {
    ParseModeGuard guard(state_, ParseMode::PSQL);

    if (check(TokenType::KW_WITH)) {
        return parseWithStatement();
    }
    if (check(TokenType::KW_IF)) {
        advance();
        return parseIfStatement();
    }
    if (check(TokenType::KW_CASE)) {
        advance();
        return parseCaseStatement();
    }
    if (checkContextual("WHILE")) {
        advance();
        return parseWhileStatement();
    }
    if (checkContextual("FOR")) {
        advance();
        return parseForStatement();
    }
    if (checkContextual("LOOP")) {
        advance();
        return parseLoopStatement();
    }
    if (checkContextual("LEAVE")) {
        advance();
        return parseLeaveStatement();
    }
    if (checkContextual("CONTINUE")) {
        advance();
        return parseContinueStatement();
    }
    if (checkContextual("IN")) {
        Token lookahead = state_.lexer().peekToken();
        if (lookahead.type == TokenType::IDENTIFIER &&
            caseInsensitiveEquals(state_.lexer().getTokenText(lookahead.span), "AUTONOMOUS")) {
            advance();  // IN
            expectContextual("AUTONOMOUS", "Expected AUTONOMOUS after IN");
            expectContextual("TRANSACTION", "Expected TRANSACTION after IN AUTONOMOUS");
            expectContextual("DO", "Expected DO after IN AUTONOMOUS TRANSACTION");
            if (check(TokenType::KW_BEGIN)) {
                advance();
                return parseBeginEndBlock();
            }
            return parsePSQLStatement();
        }
    }
    if (checkContextual("EXIT")) {
        advance();
        return parseExitStatement();
    }
    if (checkContextual("SUSPEND")) {
        advance();
        return parseSuspendStatement();
    }
    if (match(TokenType::KW_RETURN)) {
        return parseReturnStatement();
    }
    if (checkContextual("EXCEPTION")) {
        advance();
        return parseExceptionStatement();
    }
    if (checkContextual("POST_EVENT")) {
        advance();
        return parsePostEventStatement();
    }
    if (checkContextual("OPEN")) {
        advance();
        return parseOpenCursor();
    }
    if (checkContextual("FETCH")) {
        advance();
        return parseFetchCursor();
    }
    if (checkContextual("CLOSE")) {
        advance();
        return parseCloseCursor();
    }
    if (check(TokenType::KW_BEGIN)) {
        advance();
        return parseBeginEndBlock();
    }
    if (match(TokenType::KW_EXECUTE)) {
        return parseExecuteStatement();
    }
    if (match(TokenType::KW_DECLARE)) {
        if (matchContextual("VARIABLE")) {
            return parseDeclareVariable();
        }
        return parseDeclareCursor();
    }

    // Assignment: variable := expression
    if (isIdentifier()) {
        StringPool::StringId var_name = currentIdentifier();
        if (match(TokenType::COLON_EQUALS) || match(TokenType::EQUAL)) {
            auto* stmt = arena_.create<AssignmentStmt>();
            stmt->variable = var_name;
            stmt->value = parseExpression();
            return stmt;
        }
        error("Expected := for assignment or statement keyword");
        return nullptr;
    }

    // DML statements
    if (match(TokenType::KW_SELECT)) return parseSelect();
    if (match(TokenType::KW_INSERT)) return parseInsert();
    if (match(TokenType::KW_UPDATE)) return parseUpdate();
    if (match(TokenType::KW_DELETE)) return parseDelete();

    error("Expected PSQL statement");
    return nullptr;
}

Statement* Parser::parseBeginEndBlock() {
    auto* stmt = arena_.create<CompoundStmt>();

    while (!check(TokenType::KW_END) && !isAtEnd()) {
        if (check(TokenType::KW_WHEN)) {
            break;
        }

        size_t start_offset = current().span.start.offset;
        Statement* inner = parsePSQLStatement();
        if (inner) {
            stmt->statements.push_back(inner);
        } else {
            if (!isAtEnd() && current().span.start.offset == start_offset) {
                synchronize();
            }
        }

        match(TokenType::SEMICOLON);
    }

    while (check(TokenType::KW_WHEN)) {
        advance();
        Statement* handler = parseWhenStatement();
        if (handler) {
            stmt->exception_handlers.push_back(handler);
        }
    }

    expect(TokenType::KW_END, "Expected END");
    return stmt;
}

Statement* Parser::parseIfStatement() {
    auto* stmt = arena_.create<IfStmt>();

    expect(TokenType::LEFT_PAREN, "Expected '(' after IF");
    stmt->condition = parseExpression();
    expect(TokenType::RIGHT_PAREN, "Expected ')' after condition");

    expect(TokenType::KW_THEN, "Expected THEN after IF condition");

    if (check(TokenType::KW_BEGIN)) {
        advance();
        stmt->then_branch = parseBeginEndBlock();
    } else {
        stmt->then_branch = parsePSQLStatement();
    }

    if (match(TokenType::KW_ELSE)) {
        if (check(TokenType::KW_IF)) {
            advance();
            stmt->else_branch = parseIfStatement();
        } else if (check(TokenType::KW_BEGIN)) {
            advance();
            stmt->else_branch = parseBeginEndBlock();
        } else {
            stmt->else_branch = parsePSQLStatement();
        }
    }

    return stmt;
}

Statement* Parser::parseWhileStatement() {
    auto* stmt = arena_.create<WhileStmt>();

    expect(TokenType::LEFT_PAREN, "Expected '(' after WHILE");
    stmt->condition = parseExpression();
    expect(TokenType::RIGHT_PAREN, "Expected ')' after condition");

    if (!matchContextual("DO")) {
        error("Expected DO after WHILE condition");
    }

    if (check(TokenType::KW_BEGIN)) {
        advance();
        stmt->body = parseBeginEndBlock();
    } else {
        stmt->body = parsePSQLStatement();
    }

    return stmt;
}

Statement* Parser::parseCaseStatement() {
    Expression* case_expr = nullptr;
    bool simple_case = false;

    if (!check(TokenType::KW_WHEN)) {
        case_expr = parseExpression();
        simple_case = true;
    }

    struct CaseBranch {
        Expression* condition = nullptr;
        Statement* body = nullptr;
    };

    std::vector<CaseBranch> branches;
    Statement* else_branch = nullptr;

    auto parseBranchBody = [&]() -> Statement* {
        auto* block = arena_.create<CompoundStmt>();
        while (!isAtEnd()) {
            if (check(TokenType::KW_WHEN) || check(TokenType::KW_ELSE) || check(TokenType::KW_END)) {
                break;
            }
            size_t start_offset = current().span.start.offset;
            Statement* inner = parsePSQLStatement();
            if (inner) {
                block->statements.push_back(inner);
            } else {
                if (!isAtEnd() && current().span.start.offset == start_offset) {
                    synchronize();
                }
            }
            match(TokenType::SEMICOLON);
        }
        if (block->statements.empty()) {
            error("Expected statement after THEN");
            return block;
        }
        if (block->statements.size() == 1) {
            return block->statements[0];
        }
        return block;
    };

    while (match(TokenType::KW_WHEN)) {
        Expression* when_expr = parseExpression();
        if (simple_case) {
            auto* cond = arena_.create<BinaryExpr>();
            cond->op = BinaryOp::EQ;
            cond->left = case_expr;
            cond->right = when_expr;
            when_expr = cond;
        }

        expect(TokenType::KW_THEN, "Expected THEN after WHEN");
        Statement* body = parseBranchBody();
        branches.push_back({when_expr, body});
    }

    if (match(TokenType::KW_ELSE)) {
        else_branch = parseBranchBody();
    }

    expect(TokenType::KW_END, "Expected END CASE");
    if (!matchContextual("CASE")) {
        error("Expected CASE after END");
    }

    if (branches.empty()) {
        error("CASE requires at least one WHEN clause");
        return else_branch;
    }

    Statement* current = else_branch;
    for (auto it = branches.rbegin(); it != branches.rend(); ++it) {
        auto* stmt = arena_.create<IfStmt>();
        stmt->condition = it->condition;
        stmt->then_branch = it->body;
        stmt->else_branch = current;
        current = stmt;
    }

    return current;
}

Statement* Parser::parseForStatement() {
    if (check(TokenType::KW_SELECT)) {
        advance();
        auto* stmt = arena_.create<ForSelectStmt>();
        stmt->select_stmt = parseSelect();

        if (!match(TokenType::KW_INTO)) {
            error("Expected INTO after FOR SELECT");
        }
        do {
            stmt->into_variables.push_back(expectIdentifier("Expected variable name"));
        } while (match(TokenType::COMMA));

        if (!matchContextual("DO")) {
            error("Expected DO after INTO variables");
        }

        if (check(TokenType::KW_BEGIN)) {
            advance();
            stmt->body = parseBeginEndBlock();
        } else {
            stmt->body = parsePSQLStatement();
        }

        return stmt;
    }

    if (check(TokenType::KW_EXECUTE)) {
        advance();
        auto* stmt = arena_.create<ForExecuteStmt>();

        if (!matchContextual("STATEMENT")) {
            error("Expected STATEMENT after EXECUTE");
        }
        stmt->sql = parseExpression();

        if (match(TokenType::LEFT_PAREN)) {
            if (!check(TokenType::RIGHT_PAREN)) {
                do {
                    stmt->parameters.push_back(parseExpression());
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::RIGHT_PAREN, "Expected ')' after parameters");
        }

        if (match(TokenType::KW_INTO) || matchContextual("INTO")) {
            do {
                stmt->into_variables.push_back(expectIdentifier("Expected variable name"));
            } while (match(TokenType::COMMA));
        }

        if (!matchContextual("DO")) {
            error("Expected DO");
        }

        if (check(TokenType::KW_BEGIN)) {
            advance();
            stmt->body = parseBeginEndBlock();
        } else {
            stmt->body = parsePSQLStatement();
        }

        return stmt;
    }

    error("Expected SELECT or EXECUTE after FOR");
    return nullptr;
}

Statement* Parser::parseLoopStatement() {
    auto* loop = arena_.create<LoopStmt>();
    auto* body = arena_.create<CompoundStmt>();

    while (!isAtEnd()) {
        if (check(TokenType::KW_END)) {
            advance();
            if (matchContextual("LOOP")) {
                break;
            }
            error("Expected LOOP after END");
        }

        Statement* inner = parsePSQLStatement();
        if (inner) {
            body->statements.push_back(inner);
        }
        match(TokenType::SEMICOLON);
    }

    loop->body = body;
    return loop;
}

Statement* Parser::parseLeaveStatement() {
    auto* stmt = arena_.create<LeaveStmt>();
    if (isIdentifier()) {
        stmt->label = currentIdentifier();
    }
    return stmt;
}

Statement* Parser::parseContinueStatement() {
    auto* stmt = arena_.create<ContinueStmt>();
    if (isIdentifier()) {
        stmt->label = currentIdentifier();
    }
    return stmt;
}

Statement* Parser::parseExitStatement() {
    return arena_.create<ExitStmt>();
}

Statement* Parser::parseSuspendStatement() {
    return arena_.create<SuspendStmt>();
}

Statement* Parser::parseReturnStatement() {
    auto* stmt = arena_.create<ReturnStmt>();

    if (!check(TokenType::SEMICOLON) && !check(TokenType::KW_END)) {
        stmt->value = parseExpression();
    }

    return stmt;
}

Statement* Parser::parseExceptionStatement() {
    auto* stmt = arena_.create<ExceptionRaiseStmt>();
    stmt->exception_name = expectIdentifier("Expected exception name");

    if (!check(TokenType::SEMICOLON) && !check(TokenType::KW_END)) {
        stmt->message = parseExpression();
    }

    return stmt;
}

Statement* Parser::parseWhenStatement() {
    auto* stmt = arena_.create<WhenExceptionStmt>();

    if (matchContextual("ANY")) {
        stmt->type = WhenExceptionStmt::ExceptionType::ANY;
    } else if (matchContextual("SQLCODE")) {
        stmt->type = WhenExceptionStmt::ExceptionType::SQLCODE;
        if (check(TokenType::INTEGER_LITERAL)) {
            stmt->sqlcode = static_cast<int32_t>(current().value.int_value);
            advance();
        }
    } else if (matchContextual("GDSCODE")) {
        stmt->type = WhenExceptionStmt::ExceptionType::GDSCODE;
        stmt->gdscode = expectIdentifier("Expected GDSCODE identifier");
    } else if (matchContextual("EXCEPTION")) {
        stmt->type = WhenExceptionStmt::ExceptionType::EXCEPTION;
        stmt->exception_name = expectIdentifier("Expected exception name");
    } else {
        error("Expected ANY, SQLCODE, GDSCODE, or EXCEPTION after WHEN");
        return nullptr;
    }

    if (!matchContextual("DO")) {
        error("Expected DO after WHEN clause");
    }

    if (check(TokenType::KW_BEGIN)) {
        advance();
        stmt->handler = parseBeginEndBlock();
    } else {
        stmt->handler = parsePSQLStatement();
    }

    match(TokenType::SEMICOLON);
    return stmt;
}

Statement* Parser::parseDeclareVariable() {
    auto* stmt = arena_.create<DeclareVariableStmt>();

    stmt->name = expectIdentifier("Expected variable name");
    stmt->type = parseTypeName();

    if (match(TokenType::KW_NOT)) {
        expect(TokenType::KW_NULL, "Expected NULL after NOT");
        stmt->not_null = true;
    }

    if (match(TokenType::EQUAL) || match(TokenType::KW_DEFAULT)) {
        stmt->default_value = parseExpression();
    }

    return stmt;
}

// =============================================================================
// EXECUTE JOB
// =============================================================================

ExecuteJobStmt* Parser::parseExecuteJob() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<ExecuteJobStmt>();
    stmt->job_name = expectIdentifier("Expected job name");
    stmt->span = makeSpan(start);
    return stmt;
}

// =============================================================================
// CANCEL JOB RUN
// =============================================================================

CancelJobRunStmt* Parser::parseCancelJobRun() {
    SourceLocation start = currentLocation();

    auto* stmt = arena_.create<CancelJobRunStmt>();
    if (!matchContextual("RUN")) {
        error("Expected RUN after CANCEL JOB");
        stmt->span = makeSpan(start);
        return stmt;
    }

    if (check(TokenType::STRING_LITERAL)) {
        stmt->job_run_uuid = current().value.string_id;
        advance();
    } else if (isIdentifier()) {
        stmt->job_run_uuid = expectIdentifier("Expected job run UUID");
    } else {
        error("Expected job run UUID");
    }

    stmt->span = makeSpan(start);
    return stmt;
}

Statement* Parser::parseExecuteStatement() {
    if (matchContextual("BLOCK")) {
        return parseExecuteBlock();
    }
    if (matchContextual("PROCEDURE")) {
        return parseExecuteProcedure();
    }
    if (matchContextual("STATEMENT")) {
        return parseExecuteDynamicStatement();
    }

    error("Expected BLOCK, PROCEDURE, or STATEMENT after EXECUTE");
    return nullptr;
}

ExecuteBlockStmt* Parser::parseExecuteBlock() {
    auto* stmt = arena_.create<ExecuteBlockStmt>();

    if (match(TokenType::LEFT_PAREN)) {
        do {
            VariableDecl param;
            param.name = expectIdentifier("Expected parameter name");
            expect(TokenType::EQUAL, "Expected '=' in parameter");
            param.default_value = parseExpression();
            stmt->input_params.push_back(param);
        } while (match(TokenType::COMMA));
        expect(TokenType::RIGHT_PAREN, "Expected ')' after input parameters");
    }

    if (matchContextual("RETURNS")) {
        expect(TokenType::LEFT_PAREN, "Expected '(' after RETURNS");
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                VariableDecl param;
                param.name = expectIdentifier("Expected output parameter name");
                param.type = parseTypeName();
                stmt->output_params.push_back(param);
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after output parameters");
    }

    if (!match(TokenType::KW_AS) && !matchContextual("AS")) {
        error("Expected AS before EXECUTE BLOCK body");
    }

    while (check(TokenType::KW_DECLARE)) {
        advance();
        if (!matchContextual("VARIABLE")) {
            error("Expected VARIABLE after DECLARE");
            break;
        }
        VariableDecl var;
        var.name = expectIdentifier("Expected variable name");
        var.type = parseTypeName();

        if (match(TokenType::KW_NOT)) {
            expect(TokenType::KW_NULL, "Expected NULL after NOT");
            var.not_null = true;
        }

        if (match(TokenType::EQUAL) || match(TokenType::KW_DEFAULT)) {
            var.default_value = parseExpression();
        }

        stmt->variables.push_back(var);
        match(TokenType::SEMICOLON);
    }

    expect(TokenType::KW_BEGIN, "Expected BEGIN");
    stmt->body = parseBeginEndBlock();
    return stmt;
}

ExecuteProcedureStmt* Parser::parseExecuteProcedure() {
    auto* stmt = arena_.create<ExecuteProcedureStmt>();
    stmt->procedure_path = parseSchemaPath(state_);

    if (match(TokenType::LEFT_PAREN)) {
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                stmt->arguments.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after arguments");
    } else if (!checkContextual("RETURNING") &&
               !check(TokenType::SEMICOLON) && !isAtEnd()) {
        do {
            stmt->arguments.push_back(parseExpression());
        } while (match(TokenType::COMMA));
    }

    if (matchContextual("RETURNING")) {
        if (match(TokenType::KW_VALUES) || matchContextual("VALUES")) {
            // Optional VALUES keyword
        }
        do {
            stmt->returning_variables.push_back(expectIdentifier("Expected variable name"));
        } while (match(TokenType::COMMA));
    }

    return stmt;
}

ExecuteStatementStmt* Parser::parseExecuteDynamicStatement() {
    auto* stmt = arena_.create<ExecuteStatementStmt>();
    stmt->sql = parseExpression();

    if (match(TokenType::LEFT_PAREN)) {
        if (!check(TokenType::RIGHT_PAREN)) {
            do {
                stmt->parameters.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }
        expect(TokenType::RIGHT_PAREN, "Expected ')' after parameters");
    }

    while (true) {
        if (match(TokenType::KW_ON) || matchContextual("ON")) {
            expectContextual("EXTERNAL", "Expected EXTERNAL after ON");
            expectContextual("DATA", "Expected DATA after ON EXTERNAL");
            expectContextual("SOURCE", "Expected SOURCE after ON EXTERNAL DATA");
            stmt->external_data_source = parseExpression();
            continue;
        }
        if (match(TokenType::KW_AS) || matchContextual("AS")) {
            if (matchContextual("USER")) {
                stmt->as_user = parseExpression();
                continue;
            }
            error("Expected USER after AS in EXECUTE STATEMENT");
            break;
        }
        if (matchContextual("PASSWORD")) {
            stmt->password = parseExpression();
            continue;
        }
        if (matchContextual("ROLE")) {
            stmt->role = parseExpression();
            continue;
        }
        if (match(TokenType::KW_WITH) || matchContextual("WITH")) {
            if (matchContextual("AUTONOMOUS")) {
                expectContextual("TRANSACTION", "Expected TRANSACTION after WITH AUTONOMOUS");
                stmt->with_autonomous_transaction = true;
                continue;
            }
            if (matchContextual("COMMON")) {
                expectContextual("TRANSACTION", "Expected TRANSACTION after WITH COMMON");
                stmt->with_common_transaction = true;
                continue;
            }
            if (matchContextual("CALLER")) {
                expectContextual("PRIVILEGES", "Expected PRIVILEGES after WITH CALLER");
                stmt->with_caller_privileges = true;
                continue;
            }
            error("Expected AUTONOMOUS TRANSACTION, COMMON TRANSACTION, or CALLER PRIVILEGES after WITH");
            break;
        }
        break;
    }

    if (match(TokenType::KW_INTO) || matchContextual("INTO")) {
        do {
            stmt->into_variables.push_back(expectIdentifier("Expected variable name"));
        } while (match(TokenType::COMMA));
    }

    return stmt;
}

ExecuteProcedureStmt* Parser::parseCall() {
    return parseExecuteProcedure();
}

DeclareCursorStmt* Parser::parseDeclareCursor() {
    auto* stmt = arena_.create<DeclareCursorStmt>();

    if (matchContextual("CURSOR")) {
        stmt->cursor_name = expectIdentifier("Expected cursor name");
    } else {
        stmt->cursor_name = expectIdentifier("Expected cursor name");
        if (matchContextual("SCROLL")) {
            stmt->scroll = true;
        }
        if (!matchContextual("CURSOR")) {
            error("Expected CURSOR after cursor name");
        }
    }

    if (matchContextual("SCROLL")) {
        stmt->scroll = true;
    }

    if (!matchContextual("FOR")) {
        error("Expected FOR after CURSOR");
    }

    if (!check(TokenType::KW_SELECT) && !check(TokenType::KW_WITH)) {
        error("Expected SELECT after FOR");
    } else {
        stmt->select_stmt = parseSelectWithClause();
    }

    return stmt;
}

OpenCursorStmt* Parser::parseOpenCursor() {
    auto* stmt = arena_.create<OpenCursorStmt>();
    stmt->cursor_name = expectIdentifier("Expected cursor name");
    return stmt;
}

FetchCursorStmt* Parser::parseFetchCursor() {
    auto* stmt = arena_.create<FetchCursorStmt>();

    if (matchContextual("NEXT")) {
        stmt->direction = FetchCursorStmt::Direction::NEXT;
    } else if (matchContextual("PRIOR")) {
        stmt->direction = FetchCursorStmt::Direction::PRIOR;
    } else if (matchContextual("FIRST")) {
        stmt->direction = FetchCursorStmt::Direction::FIRST;
    } else if (matchContextual("LAST")) {
        stmt->direction = FetchCursorStmt::Direction::LAST;
    } else if (matchContextual("ABSOLUTE")) {
        stmt->direction = FetchCursorStmt::Direction::ABSOLUTE;
        stmt->offset = parseExpression();
    } else if (matchContextual("RELATIVE")) {
        stmt->direction = FetchCursorStmt::Direction::RELATIVE;
        stmt->offset = parseExpression();
    }

    stmt->cursor_name = expectIdentifier("Expected cursor name");

    if (match(TokenType::KW_INTO) || matchContextual("INTO")) {
        do {
            stmt->into_variables.push_back(expectIdentifier("Expected variable name"));
        } while (match(TokenType::COMMA));
    }

    return stmt;
}

CloseCursorStmt* Parser::parseCloseCursor() {
    auto* stmt = arena_.create<CloseCursorStmt>();
    stmt->cursor_name = expectIdentifier("Expected cursor name");
    return stmt;
}

PostEventStmt* Parser::parsePostEventStatement() {
    auto* stmt = arena_.create<PostEventStmt>();
    stmt->event_name = parseExpression();
    return stmt;
}

} // namespace scratchbird::parser::v3
