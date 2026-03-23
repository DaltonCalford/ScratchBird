#include "scratchbird/udr/dialect_compiler_udr.h"

#include "scratchbird/sblr/firebird_query_compiler.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace scratchbird::sblr
{

namespace
{

constexpr const char *kDialectCompilerContractId = "sb_dialect_compiler_udr/v1";

auto isZeroUuid(const core::ID &id) -> bool
{
    for (uint8_t byte : id.bytes)
    {
        if (byte != 0)
        {
            return false;
        }
    }
    return true;
}

auto setCompileError(core::ErrorContext *ctx,
                     DialectCompilerResponse &response_out,
                     core::Status status,
                     const std::string &message) -> core::Status
{
    response_out.errors.push_back(message);
    if (ctx != nullptr)
    {
        ctx->set(status, message.c_str(), __FILE__, __LINE__, __func__);
    }
    return status;
}

struct FirebirdBlrSubsetQuery
{
    std::string from_clause = "\"RDB$DATABASE\"";
    std::vector<std::string> select_items;
    std::optional<std::string> where_clause;
    std::optional<std::string> limit_clause;
};

class FirebirdBlrSubsetDecoder
{
public:
    explicit FirebirdBlrSubsetDecoder(const std::vector<uint8_t> &blr)
        : blr_(blr)
    {
    }

    auto decodeToSql(std::string &sql_out, std::string &error_out) -> bool
    {
        if (blr_.empty())
        {
            error_out = "Firebird BLR payload is empty";
            return false;
        }

        const uint8_t version = readByte(error_out);
        if (!error_out.empty())
        {
            return false;
        }
        if (version != 4 && version != 5)
        {
            error_out = "Unsupported Firebird BLR version";
            return false;
        }

        if (!expectByte(2, error_out))
        {
            return false;
        }

        while (!eof() && peekByte(error_out) == 4)
        {
            parseMessage(error_out);
            if (!error_out.empty())
            {
                return false;
            }
        }

        FirebirdBlrSubsetQuery query;
        if (!parseStatement(query, std::nullopt, error_out))
        {
            return false;
        }

        if (!expectByte(255, error_out))
        {
            return false;
        }

        if (!eof())
        {
            if (!expectByte(76, error_out))
            {
                return false;
            }
            if (!eof())
            {
                error_out = "Unexpected bytes after Firebird BLR end-of-command";
                return false;
            }
        }

        if (query.select_items.empty())
        {
            error_out = "Firebird BLR subset lowering produced an empty projection";
            return false;
        }

        std::ostringstream sql;
        sql << "SELECT ";
        for (size_t i = 0; i < query.select_items.size(); ++i)
        {
            if (i != 0)
            {
                sql << ", ";
            }
            sql << query.select_items[i];
        }
        sql << " FROM " << query.from_clause;
        if (query.where_clause.has_value())
        {
            sql << " WHERE " << *query.where_clause;
        }
        if (query.limit_clause.has_value())
        {
            sql << " ROWS " << *query.limit_clause;
        }
        sql_out = sql.str();
        return true;
    }

private:
    const std::vector<uint8_t> &blr_;
    size_t pos_ = 0;
    uint8_t next_context_ = 0;
    std::unordered_map<uint8_t, std::string> context_sql_names_;

    auto eof() const -> bool
    {
        return pos_ >= blr_.size();
    }

    auto readByte(std::string &error_out) -> uint8_t
    {
        if (pos_ >= blr_.size())
        {
            error_out = "Firebird BLR truncated";
            return 0;
        }
        return blr_[pos_++];
    }

    auto peekByte(std::string &error_out) const -> uint8_t
    {
        if (pos_ >= blr_.size())
        {
            error_out = "Firebird BLR truncated";
            return 0;
        }
        return blr_[pos_];
    }

    auto readWord(std::string &error_out) -> uint16_t
    {
        const uint16_t lo = readByte(error_out);
        if (!error_out.empty())
        {
            return 0;
        }
        const uint16_t hi = readByte(error_out);
        if (!error_out.empty())
        {
            return 0;
        }
        return static_cast<uint16_t>(lo | (hi << 8));
    }

    auto readInt16(std::string &error_out) -> int16_t
    {
        return static_cast<int16_t>(readWord(error_out));
    }

    auto readInt32(std::string &error_out) -> int32_t
    {
        const uint32_t b1 = readByte(error_out);
        if (!error_out.empty())
        {
            return 0;
        }
        const uint32_t b2 = readByte(error_out);
        if (!error_out.empty())
        {
            return 0;
        }
        const uint32_t b3 = readByte(error_out);
        if (!error_out.empty())
        {
            return 0;
        }
        const uint32_t b4 = readByte(error_out);
        if (!error_out.empty())
        {
            return 0;
        }
        return static_cast<int32_t>(b1 | (b2 << 8) | (b3 << 16) | (b4 << 24));
    }

    auto readInt64(std::string &error_out) -> int64_t
    {
        uint64_t value = 0;
        for (unsigned shift = 0; shift < 64; shift += 8)
        {
            value |= static_cast<uint64_t>(readByte(error_out)) << shift;
            if (!error_out.empty())
            {
                return 0;
            }
        }
        return static_cast<int64_t>(value);
    }

    auto readString8(std::string &error_out) -> std::string
    {
        const uint8_t len = readByte(error_out);
        if (!error_out.empty())
        {
            return {};
        }
        if (pos_ + len > blr_.size())
        {
            error_out = "Firebird BLR truncated while reading string";
            return {};
        }
        std::string value(reinterpret_cast<const char *>(blr_.data() + pos_), len);
        pos_ += len;
        return value;
    }

    auto expectByte(uint8_t expected, std::string &error_out) -> bool
    {
        const uint8_t actual = readByte(error_out);
        if (!error_out.empty())
        {
            return false;
        }
        if (actual != expected)
        {
            std::ostringstream message;
            message << "Unexpected Firebird BLR opcode " << static_cast<unsigned>(actual)
                    << ", expected " << static_cast<unsigned>(expected);
            error_out = message.str();
            return false;
        }
        return true;
    }

    static auto quoteIdentifier(const std::string &identifier) -> std::string
    {
        std::string quoted = "\"";
        for (char ch : identifier)
        {
            if (ch == '"')
            {
                quoted += "\"\"";
            }
            else
            {
                quoted.push_back(ch);
            }
        }
        quoted.push_back('"');
        return quoted;
    }

    static auto quoteStringLiteral(const std::string &value) -> std::string
    {
        std::string quoted = "'";
        for (char ch : value)
        {
            if (ch == '\'')
            {
                quoted += "''";
            }
            else
            {
                quoted.push_back(ch);
            }
        }
        quoted.push_back('\'');
        return quoted;
    }

    static auto renderScaledInteger(int64_t value, int8_t scale) -> std::string
    {
        if (scale >= 0)
        {
            return std::to_string(value);
        }

        const bool negative = value < 0;
        uint64_t abs_value = static_cast<uint64_t>(negative ? -value : value);
        std::string digits = std::to_string(abs_value);
        const size_t frac_digits = static_cast<size_t>(-scale);
        if (digits.size() <= frac_digits)
        {
            digits.insert(digits.begin(), frac_digits - digits.size() + 1, '0');
        }
        digits.insert(digits.end() - static_cast<std::ptrdiff_t>(frac_digits), '.');
        if (negative)
        {
            digits.insert(digits.begin(), '-');
        }
        return digits;
    }

    void parseMessage(std::string &error_out)
    {
        (void)readByte(error_out);
        if (!error_out.empty())
        {
            return;
        }
        (void)readByte(error_out);
        const uint16_t field_count = readWord(error_out);
        if (!error_out.empty())
        {
            return;
        }
        for (uint16_t i = 0; i < field_count; ++i)
        {
            skipTypeDescriptor(error_out);
            if (!error_out.empty())
            {
                return;
            }
        }
    }

    void skipTypeDescriptor(std::string &error_out)
    {
        const uint8_t opcode = readByte(error_out);
        if (!error_out.empty())
        {
            return;
        }

        switch (opcode)
        {
        case 20:
            skipTypeDescriptor(error_out);
            return;
        case 7:
        case 8:
        case 16:
        case 24:
        case 25:
        case 26:
            (void)readByte(error_out);
            return;
        case 10:
        case 12:
        case 13:
        case 23:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
        case 35:
            return;
        case 14:
        case 15:
        case 37:
        case 38:
        case 40:
        case 41:
            (void)readWord(error_out);
            if (!error_out.empty() && (opcode == 15 || opcode == 38 || opcode == 41))
            {
                return;
            }
            if (opcode == 15 || opcode == 38 || opcode == 41)
            {
                (void)readWord(error_out);
            }
            return;
        default:
            error_out = "Unsupported Firebird BLR message datatype opcode " +
                        std::to_string(opcode);
            return;
        }
    }

    auto parseStatement(FirebirdBlrSubsetQuery &query_out,
                        const std::optional<FirebirdBlrSubsetQuery> &base_query,
                        std::string &error_out) -> bool
    {
        const uint8_t opcode = readByte(error_out);
        if (!error_out.empty())
        {
            return false;
        }

        switch (opcode)
        {
        case 2:
        {
            bool found_query = false;
            while (true)
            {
                const uint8_t next = peekByte(error_out);
                if (!error_out.empty())
                {
                    return false;
                }
                if (next == 255)
                {
                    (void)readByte(error_out);
                    return found_query;
                }

                FirebirdBlrSubsetQuery nested;
                if (!parseStatement(nested, base_query, error_out))
                {
                    return false;
                }
                if (!nested.select_items.empty())
                {
                    query_out = std::move(nested);
                    found_query = true;
                }
            }
        }
        case 12:
            (void)readByte(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            return parseStatement(query_out, base_query, error_out);
        case 14:
            return parseSend(query_out, base_query, error_out);
        case 7:
            return parseFor(query_out, error_out);
        default:
            error_out = "Unsupported Firebird BLR statement opcode " + std::to_string(opcode);
            return false;
        }
    }

    auto parseFor(FirebirdBlrSubsetQuery &query_out, std::string &error_out) -> bool
    {
        FirebirdBlrSubsetQuery base_query;
        if (!parseRse(base_query, error_out))
        {
            return false;
        }
        return parseStatement(query_out, base_query, error_out);
    }

    auto parseSend(FirebirdBlrSubsetQuery &query_out,
                   const std::optional<FirebirdBlrSubsetQuery> &base_query,
                   std::string &error_out) -> bool
    {
        (void)readByte(error_out);
        if (!error_out.empty())
        {
            return false;
        }

        FirebirdBlrSubsetQuery working = base_query.value_or(FirebirdBlrSubsetQuery{});
        const uint8_t next = peekByte(error_out);
        if (!error_out.empty())
        {
            return false;
        }

        if (next == 2)
        {
            (void)readByte(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            while (true)
            {
                const uint8_t block_opcode = peekByte(error_out);
                if (!error_out.empty())
                {
                    return false;
                }
                if (block_opcode == 255)
                {
                    (void)readByte(error_out);
                    break;
                }
                if (!parseAssignmentIntoProjection(working, error_out))
                {
                    return false;
                }
            }
        }
        else
        {
            if (!parseAssignmentIntoProjection(working, error_out))
            {
                return false;
            }
        }

        query_out = std::move(working);
        return true;
    }

    auto parseAssignmentIntoProjection(FirebirdBlrSubsetQuery &query,
                                       std::string &error_out) -> bool
    {
        if (!expectByte(1, error_out))
        {
            return false;
        }

        std::string expr;
        if (!parseValueExpr(expr, error_out))
        {
            return false;
        }

        const uint8_t target_opcode = readByte(error_out);
        if (!error_out.empty())
        {
            return false;
        }
        if (target_opcode == 25)
        {
            (void)readByte(error_out);
            (void)readWord(error_out);
        }
        else if (target_opcode == 41)
        {
            (void)readByte(error_out);
            (void)readWord(error_out);
            (void)readWord(error_out);
        }
        else
        {
            error_out = "Unsupported Firebird BLR assignment target opcode " +
                        std::to_string(target_opcode);
            return false;
        }

        query.select_items.push_back(std::move(expr));
        return true;
    }

    auto parseRse(FirebirdBlrSubsetQuery &query, std::string &error_out) -> bool
    {
        if (!expectByte(67, error_out))
        {
            return false;
        }

        const uint8_t source_count = readByte(error_out);
        if (!error_out.empty())
        {
            return false;
        }
        if (source_count != 1)
        {
            error_out = "Firebird BLR subset only supports single-relation record selection expressions";
            return false;
        }

        std::string relation_sql;
        if (!parseRelationSource(relation_sql, error_out))
        {
            return false;
        }
        query.from_clause = relation_sql;

        while (true)
        {
            const uint8_t opcode = readByte(error_out);
            if (!error_out.empty())
            {
                return false;
            }

            switch (opcode)
            {
            case 71:
            {
                std::string boolean_sql;
                if (!parseBooleanExpr(boolean_sql, error_out))
                {
                    return false;
                }
                query.where_clause = boolean_sql;
                break;
            }
            case 68:
            {
                std::string limit_sql;
                if (!parseValueExpr(limit_sql, error_out))
                {
                    return false;
                }
                query.limit_clause = limit_sql;
                break;
            }
            case 255:
                return true;
            default:
                error_out = "Unsupported Firebird BLR RSE clause opcode " +
                            std::to_string(opcode);
                return false;
            }
        }
    }

    auto parseRelationSource(std::string &relation_sql, std::string &error_out) -> bool
    {
        const uint8_t opcode = readByte(error_out);
        if (!error_out.empty())
        {
            return false;
        }

        std::string schema_name;
        std::string relation_name;
        std::string alias_name;
        switch (opcode)
        {
        case 74:
            relation_name = readString8(error_out);
            break;
        case 146:
            relation_name = readString8(error_out);
            alias_name = readString8(error_out);
            break;
        case 148:
            schema_name = readString8(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            relation_name = readString8(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            alias_name = readString8(error_out);
            break;
        default:
            error_out = "Unsupported Firebird BLR relation source opcode " +
                        std::to_string(opcode);
            return false;
        }

        if (!error_out.empty())
        {
            return false;
        }

        std::string qualified_relation = quoteIdentifier(relation_name);
        if (!schema_name.empty())
        {
            qualified_relation = quoteIdentifier(schema_name) + "." + qualified_relation;
        }

        const uint8_t context = next_context_++;
        std::string context_sql = qualified_relation;
        if (!alias_name.empty())
        {
            context_sql = quoteIdentifier(alias_name);
            relation_sql = qualified_relation + " " + context_sql;
        }
        else
        {
            relation_sql = qualified_relation;
        }
        context_sql_names_[context] = context_sql;
        return true;
    }

    auto parseBooleanExpr(std::string &expr_out, std::string &error_out) -> bool
    {
        const uint8_t opcode = readByte(error_out);
        if (!error_out.empty())
        {
            return false;
        }

        switch (opcode)
        {
        case 47:
        case 48:
        case 49:
        case 50:
        case 51:
        case 52:
        {
            std::string lhs;
            std::string rhs;
            if (!parseValueExpr(lhs, error_out) || !parseValueExpr(rhs, error_out))
            {
                return false;
            }
            const char *sql_op = "=";
            switch (opcode)
            {
            case 48:
                sql_op = "<>";
                break;
            case 49:
                sql_op = ">";
                break;
            case 50:
                sql_op = ">=";
                break;
            case 51:
                sql_op = "<";
                break;
            case 52:
                sql_op = "<=";
                break;
            default:
                break;
            }
            expr_out = "(" + lhs + " " + sql_op + " " + rhs + ")";
            return true;
        }
        case 58:
        case 57:
        {
            std::string lhs;
            std::string rhs;
            if (!parseBooleanExpr(lhs, error_out) || !parseBooleanExpr(rhs, error_out))
            {
                return false;
            }
            expr_out = "(" + lhs + (opcode == 58 ? " AND " : " OR ") + rhs + ")";
            return true;
        }
        case 59:
        {
            std::string inner;
            if (!parseBooleanExpr(inner, error_out))
            {
                return false;
            }
            expr_out = "(NOT " + inner + ")";
            return true;
        }
        default:
            error_out = "Unsupported Firebird BLR boolean opcode " + std::to_string(opcode);
            return false;
        }
    }

    auto parseValueExpr(std::string &expr_out, std::string &error_out) -> bool
    {
        const uint8_t opcode = readByte(error_out);
        if (!error_out.empty())
        {
            return false;
        }

        switch (opcode)
        {
        case 23:
        {
            const uint8_t context = readByte(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            const std::string field_name = readString8(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            auto it = context_sql_names_.find(context);
            if (it == context_sql_names_.end())
            {
                error_out = "Unknown Firebird BLR field context " + std::to_string(context);
                return false;
            }
            expr_out = it->second + "." + quoteIdentifier(field_name);
            return true;
        }
        case 21:
            return parseLiteral(expr_out, error_out);
        case 45:
            expr_out = "NULL";
            return true;
        case 25:
        {
            (void)readByte(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            (void)readWord(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            expr_out = "?";
            return true;
        }
        case 41:
        {
            (void)readByte(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            (void)readWord(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            (void)readWord(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            expr_out = "?";
            return true;
        }
        default:
            error_out = "Unsupported Firebird BLR value opcode " + std::to_string(opcode);
            return false;
        }
    }

    auto parseLiteral(std::string &literal_out, std::string &error_out) -> bool
    {
        const uint8_t type_opcode = readByte(error_out);
        if (!error_out.empty())
        {
            return false;
        }

        switch (type_opcode)
        {
        case 7:
        {
            const int8_t scale = static_cast<int8_t>(readByte(error_out));
            const int16_t value = readInt16(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            literal_out = renderScaledInteger(value, scale);
            return true;
        }
        case 8:
        {
            const int8_t scale = static_cast<int8_t>(readByte(error_out));
            const int32_t value = readInt32(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            literal_out = renderScaledInteger(value, scale);
            return true;
        }
        case 16:
        {
            const int8_t scale = static_cast<int8_t>(readByte(error_out));
            const int64_t value = readInt64(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            literal_out = renderScaledInteger(value, scale);
            return true;
        }
        case 14:
        {
            const uint16_t len = readWord(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            if (pos_ + len > blr_.size())
            {
                error_out = "Firebird BLR truncated while reading text literal";
                return false;
            }
            std::string value(reinterpret_cast<const char *>(blr_.data() + pos_), len);
            pos_ += len;
            literal_out = quoteStringLiteral(value);
            return true;
        }
        case 37:
        case 38:
        {
            const uint16_t len = readWord(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            if (pos_ + len > blr_.size())
            {
                error_out = "Firebird BLR truncated while reading varying literal";
                return false;
            }
            std::string value(reinterpret_cast<const char *>(blr_.data() + pos_), len);
            pos_ += len;
            literal_out = quoteStringLiteral(value);
            return true;
        }
        case 23:
        {
            const uint8_t value = readByte(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            literal_out = value != 0 ? "TRUE" : "FALSE";
            return true;
        }
        case 27:
        {
            const uint16_t len = readWord(error_out);
            if (!error_out.empty())
            {
                return false;
            }
            if (pos_ + len > blr_.size())
            {
                error_out = "Firebird BLR truncated while reading numeric literal";
                return false;
            }
            literal_out.assign(reinterpret_cast<const char *>(blr_.data() + pos_), len);
            pos_ += len;
            return true;
        }
        default:
            error_out = "Unsupported Firebird BLR literal type opcode " +
                        std::to_string(type_opcode);
            return false;
        }
    }
};

auto finalizeCompilation(FirebirdQueryCompiler &compiler,
                         const std::string &sql,
                         DialectCompilerResponse &response_out,
                         core::ErrorContext *ctx,
                         bool emit_blr_warning) -> core::Status
{
    auto result = compiler.compile(sql);
    response_out.warnings = result.warnings();
    if (emit_blr_warning)
    {
        response_out.warnings.push_back(
            "Firebird BLR was lowered through the executable-BLR subset translator");
    }
    response_out.errors = result.errors();
    if (!result.success())
    {
        if (ctx != nullptr && !response_out.errors.empty())
        {
            ctx->set(core::Status::INVALID_ARGUMENT,
                     response_out.errors.front().c_str(),
                     __FILE__, __LINE__, __func__);
        }
        return core::Status::INVALID_ARGUMENT;
    }

    response_out.bytecode = result.bytecode();
    response_out.success = true;
    return core::Status::OK;
}

} // namespace

auto compileFirebirdDialectToSblr(core::Database *db,
                                  const DialectCompilerRequest &request,
                                  DialectCompilerResponse &response_out,
                                  core::ErrorContext *ctx) -> core::Status
{
    response_out = DialectCompilerResponse{};
    response_out.contract_id = kDialectCompilerContractId;
    response_out.profile_id =
        request.session.profile_id.empty() ? "firebirdsql" : request.session.profile_id;
    response_out.module_name = request.module_name;
    response_out.native_feature_key =
        request.payload_format == DialectCompilerPayloadFormat::FIREBIRD_BLR
            ? "compile_blr_to_sblr"
            : "compile_sql_to_sblr";

    if (request.payload.empty())
    {
        return setCompileError(ctx,
                               response_out,
                               core::Status::INVALID_ARGUMENT,
                               "Dialect compiler request payload is empty");
    }

    FirebirdQueryCompiler compiler(db);
    if (!isZeroUuid(request.session.current_schema_id))
    {
        compiler.setCurrentSchema(request.session.current_schema_id);
    }
    if (!request.session.current_schema_name.empty())
    {
        compiler.setDefaultSchema(request.session.current_schema_name);
    }
    if (!request.session.search_path.empty())
    {
        compiler.setSearchPath(request.session.search_path);
    }
    compiler.setOptimizationsEnabled(request.optimizations_enabled);
    compiler.setStatsEnabled(request.stats_enabled);

    if (request.payload_format == DialectCompilerPayloadFormat::FIREBIRD_BLR)
    {
        std::string sql;
        std::string blr_error;
        FirebirdBlrSubsetDecoder decoder(request.payload);
        if (!decoder.decodeToSql(sql, blr_error))
        {
            return setCompileError(ctx,
                                   response_out,
                                   core::Status::NOT_SUPPORTED,
                                   blr_error);
        }
        return finalizeCompilation(compiler, sql, response_out, ctx, true);
    }

    if (request.payload_format != DialectCompilerPayloadFormat::SQL_TEXT)
    {
        return setCompileError(ctx,
                               response_out,
                               core::Status::NOT_SUPPORTED,
                               "Firebird compiler only accepts SQL_TEXT or FIREBIRD_BLR payloads");
    }

    const std::string sql(request.payload.begin(), request.payload.end());
    return finalizeCompilation(compiler, sql, response_out, ctx, false);
}

} // namespace scratchbird::sblr
