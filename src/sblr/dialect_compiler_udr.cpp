/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/udr/dialect_compiler_udr.h"

#include "scratchbird/core/database.h"
#include "scratchbird/core/emulation_package_manifest.h"
#include "scratchbird/sblr/firebird_query_compiler.h"
#include "scratchbird/sblr/mysql_query_compiler.h"
#include "scratchbird/sblr/postgresql_query_compiler.h"
#include "scratchbird/sblr/query_compiler_v3.h"

#include <algorithm>
#include <cctype>
#include <mutex>
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

        auto setError(core::ErrorContext *ctx,
                      core::Status status,
                      const std::string &message) -> core::Status
        {
            if (ctx != nullptr)
            {
                ctx->set(status, message.c_str(), __FILE__, __LINE__, __func__);
            }
            return status;
        }

        auto toLowerAscii(std::string value) -> std::string
        {
            std::transform(value.begin(), value.end(), value.begin(),
                           [](unsigned char ch) {
                               return static_cast<char>(std::tolower(ch));
                           });
            return value;
        }

        auto joinSearchPath(const std::vector<std::string> &search_path) -> std::string
        {
            std::ostringstream out;
            for (size_t i = 0; i < search_path.size(); ++i)
            {
                if (i != 0)
                {
                    out << ':';
                }
                out << search_path[i];
            }
            return out.str();
        }

        auto canonicalizeProfileId(const std::string &profile_id,
                                   const std::string &module_name) -> std::string
        {
            std::string normalized = toLowerAscii(profile_id.empty() ? module_name : profile_id);
            if (normalized == "scratchbird_v3" || normalized == "scratchbird_native" ||
                normalized == "native")
            {
                return "scratchbird";
            }
            if (normalized == "firebird" || normalized == "firebird_emulation")
            {
                return "firebirdsql";
            }
            if (normalized == "postgresql_emulation")
            {
                return "postgresql";
            }
            if (normalized == "mysql_emulation")
            {
                return "mysql";
            }
            return normalized;
        }

        auto featureKeyForRequest(const DialectCompilerRequest &request) -> std::string
        {
            if (request.payload_format == DialectCompilerPayloadFormat::FIREBIRD_BLR)
            {
                return "compile_blr_to_sblr";
            }
            if (request.session.engine_dynamic_sql)
            {
                return "engine_dynamic_sql_compile";
            }
            return "compile_sql_to_sblr";
        }

        auto buildSessionOptionSignature(const DialectCompilerRequest &request) -> std::string
        {
            std::ostringstream out;
            out << "dialect=" << request.session.dialect_tag
                << ";profile=" << request.session.profile_id
                << ";schema=" << request.session.current_schema_name
                << ";root=" << request.session.emulated_schema_root
                << ";search=" << joinSearchPath(request.session.search_path);
            return out.str();
        }

        auto buildRoleContextSignature(const DialectCompilerRequest &request) -> std::string
        {
            std::ostringstream out;
            out << "principal=" << request.session.principal_id.toString()
                << ";role=" << request.session.active_role_id.toString()
                << ";auth=" << request.session.auth_key_id.toString();
            return out.str();
        }

        auto makeInstallDescriptors() -> std::vector<DialectCompilerInstallDescriptor>
        {
            std::vector<DialectCompilerInstallDescriptor> descriptors;
            for (const auto &manifest : core::builtinEmulationPackageManifests())
            {
                if (manifest.kind != core::EmulationPackageKind::COMPILER_UDR)
                {
                    continue;
                }
                std::vector<std::string> feature_keys{
                    "compile_sql_to_sblr",
                    "engine_dynamic_sql_compile"
                };
                if (manifest.profile_id == "firebirdsql")
                {
                    feature_keys.push_back("compile_blr_to_sblr");
                }
                descriptors.push_back({manifest.profile_id,
                                       "1.0",
                                       manifest.package_name,
                                       "SQL_REWRITE_TO_NATIVE",
                                       std::move(feature_keys)});
            }
            return descriptors;
        }

        void seedRegistry(udr::LanguageUdrRegistry &registry)
        {
            for (const auto &descriptor : builtinDialectCompilerInstallDescriptors())
            {
                udr::LanguageUdrRegistration registration{};
                registration.module_id = core::generateUuidV7();
                registration.module_name = descriptor.module_name;
                registration.engine_profile_id = descriptor.profile_id;
                registration.engine_profile_version = descriptor.profile_version;
                registration.translation_mode = descriptor.translation_mode;
                registration.module_semver = "1.0.0";
                registration.artifact_hash = "artifact_" + descriptor.profile_id;
                registration.signature_status = udr::LanguageUdrSignatureStatus::TRUSTED;
                registration.status = udr::LanguageUdrModuleStatus::ACTIVE;

                core::ErrorContext register_ctx;
                (void)registry.registerModule(registration, descriptor.feature_keys, &register_ctx);
            }
        }

        auto compileFirebird(core::Database *db,
                             const DialectCompilerRequest &request,
                             DialectCompilerResponse &response_out) -> core::Status
        {
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
                    (void)readByte(error_out); // blr_message
                    if (!error_out.empty())
                    {
                        return;
                    }
                    (void)readByte(error_out); // message number
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
                    case 20: // blr_not_nullable
                        skipTypeDescriptor(error_out);
                        return;
                    case 7:  // blr_short
                    case 8:  // blr_long
                    case 16: // blr_int64
                    case 24: // blr_dec64
                    case 25: // blr_dec128
                    case 26: // blr_int128
                        (void)readByte(error_out);
                        return;
                    case 10: // blr_float
                    case 12: // blr_sql_date
                    case 13: // blr_sql_time
                    case 23: // blr_bool
                    case 27: // blr_double
                    case 28: // blr_sql_time_tz
                    case 29: // blr_timestamp_tz
                    case 30: // blr_ex_time_tz
                    case 31: // blr_ex_timestamp_tz
                    case 35: // blr_timestamp
                        return;
                    case 14: // blr_text
                    case 15: // blr_text2
                    case 37: // blr_varying
                    case 38: // blr_varying2
                    case 40: // blr_cstring
                    case 41: // blr_cstring2
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
                    case 2: // blr_begin
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
                    case 12: // blr_receive
                        (void)readByte(error_out); // message number
                        if (!error_out.empty())
                        {
                            return false;
                        }
                        return parseStatement(query_out, base_query, error_out);
                    case 14: // blr_send
                        return parseSend(query_out, base_query, error_out);
                    case 7: // blr_for
                        return parseFor(query_out, error_out);
                    default:
                        error_out = "Unsupported Firebird BLR statement opcode " +
                                    std::to_string(opcode);
                        return false;
                    }
                }

                auto parseFor(FirebirdBlrSubsetQuery &query_out,
                              std::string &error_out) -> bool
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
                    (void)readByte(error_out); // output message number
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

                    if (next == 2) // blr_begin
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
                    if (!expectByte(1, error_out)) // blr_assignment
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
                    if (target_opcode == 25) // blr_parameter
                    {
                        (void)readByte(error_out);
                        (void)readWord(error_out);
                    }
                    else if (target_opcode == 41) // blr_parameter2
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
                    if (!expectByte(67, error_out)) // blr_rse
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
                        case 71: // blr_boolean
                        {
                            std::string boolean_sql;
                            if (!parseBooleanExpr(boolean_sql, error_out))
                            {
                                return false;
                            }
                            query.where_clause = boolean_sql;
                            break;
                        }
                        case 68: // blr_first
                        {
                            std::string limit_sql;
                            if (!parseValueExpr(limit_sql, error_out))
                            {
                                return false;
                            }
                            query.limit_clause = limit_sql;
                            break;
                        }
                        case 255: // blr_end
                            return true;
                        default:
                            error_out = "Unsupported Firebird BLR RSE clause opcode " +
                                        std::to_string(opcode);
                            return false;
                        }
                    }
                }

                auto parseRelationSource(std::string &relation_sql,
                                         std::string &error_out) -> bool
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
                    case 74: // blr_relation
                        relation_name = readString8(error_out);
                        break;
                    case 146: // blr_relation2
                        relation_name = readString8(error_out);
                        alias_name = readString8(error_out);
                        break;
                    case 148: // blr_relation3
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

                auto parseBooleanExpr(std::string &expr_out,
                                      std::string &error_out) -> bool
                {
                    const uint8_t opcode = readByte(error_out);
                    if (!error_out.empty())
                    {
                        return false;
                    }

                    switch (opcode)
                    {
                    case 47: // blr_eql
                    case 48: // blr_neq
                    case 49: // blr_gtr
                    case 50: // blr_geq
                    case 51: // blr_lss
                    case 52: // blr_leq
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
                    case 58: // blr_and
                    case 57: // blr_or
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
                    case 59: // blr_not
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
                        error_out = "Unsupported Firebird BLR boolean opcode " +
                                    std::to_string(opcode);
                        return false;
                    }
                }

                auto parseValueExpr(std::string &expr_out,
                                    std::string &error_out) -> bool
                {
                    const uint8_t opcode = readByte(error_out);
                    if (!error_out.empty())
                    {
                        return false;
                    }

                    switch (opcode)
                    {
                    case 23: // blr_field
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
                    case 21: // blr_literal
                        return parseLiteral(expr_out, error_out);
                    case 45: // blr_null
                        expr_out = "NULL";
                        return true;
                    case 25: // blr_parameter
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
                    case 41: // blr_parameter2
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
                        error_out = "Unsupported Firebird BLR value opcode " +
                                    std::to_string(opcode);
                        return false;
                    }
                }

                auto parseLiteral(std::string &literal_out,
                                  std::string &error_out) -> bool
                {
                    const uint8_t type_opcode = readByte(error_out);
                    if (!error_out.empty())
                    {
                        return false;
                    }

                    switch (type_opcode)
                    {
                    case 7: // blr_short
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
                    case 8: // blr_long
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
                    case 16: // blr_int64
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
                    case 14: // blr_text
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
                    case 37: // blr_varying
                    case 38: // blr_varying2
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
                    case 23: // blr_bool
                    {
                        const uint8_t value = readByte(error_out);
                        if (!error_out.empty())
                        {
                            return false;
                        }
                        literal_out = value != 0 ? "TRUE" : "FALSE";
                        return true;
                    }
                    case 27: // blr_double
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

            if (request.payload_format == DialectCompilerPayloadFormat::FIREBIRD_BLR)
            {
                std::string sql;
                std::string blr_error;
                FirebirdBlrSubsetDecoder decoder(request.payload);
                if (!decoder.decodeToSql(sql, blr_error))
                {
                    response_out.errors.push_back(blr_error);
                    return core::Status::NOT_SUPPORTED;
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

                auto result = compiler.compile(sql);
                response_out.warnings = result.warnings();
                response_out.warnings.push_back(
                    "Firebird BLR was lowered through the executable-BLR subset translator");
                response_out.errors = result.errors();
                if (result.success())
                {
                    response_out.bytecode = result.bytecode();
                    response_out.success = true;
                    return core::Status::OK;
                }
                return core::Status::INVALID_ARGUMENT;
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

            const std::string sql(request.payload.begin(), request.payload.end());
            auto result = compiler.compile(sql);
            response_out.warnings = result.warnings();
            response_out.errors = result.errors();
            if (result.success())
            {
                response_out.bytecode = result.bytecode();
                response_out.success = true;
                return core::Status::OK;
            }
            return core::Status::INVALID_ARGUMENT;
        }

        auto compileScratchBird(core::Database *db,
                                const DialectCompilerRequest &request,
                                DialectCompilerResponse &response_out) -> core::Status
        {
            if (request.payload_format != DialectCompilerPayloadFormat::SQL_TEXT)
            {
                response_out.errors.push_back(
                    "ScratchBird compiler UDR only accepts SQL_TEXT payloads");
                return core::Status::NOT_SUPPORTED;
            }

            if (db == nullptr)
            {
                response_out.errors.push_back(
                    "ScratchBird compiler UDR requires database context");
                return core::Status::INVALID_ARGUMENT;
            }

            const std::string sql(request.payload.begin(), request.payload.end());
            QueryCompilerV3 compiler(db);
            if (!isZeroUuid(request.session.current_schema_id))
            {
                compiler.setCurrentSchema(request.session.current_schema_id);
            }
            compiler.setOptimizationsEnabled(request.optimizations_enabled);
            compiler.setStatsEnabled(request.stats_enabled);
            auto result = compiler.compile(sql);
            response_out.warnings = result.warnings();
            response_out.errors = result.errors();
            if (!result.success())
            {
                if (response_out.errors.empty())
                {
                    response_out.errors.push_back("ScratchBird SQL to SBLR lowering failed");
                }
                return core::Status::INVALID_ARGUMENT;
            }

            response_out.bytecode = result.bytecode();
            response_out.success = true;
            return core::Status::OK;
        }

        auto compilePostgreSql(core::Database *db,
                               const DialectCompilerRequest &request,
                               DialectCompilerResponse &response_out) -> core::Status
        {
            if (request.payload_format != DialectCompilerPayloadFormat::SQL_TEXT)
            {
                response_out.errors.push_back(
                    "PostgreSQL compiler UDR only accepts SQL_TEXT payloads");
                return core::Status::NOT_SUPPORTED;
            }

            PostgreSQLQueryCompiler compiler(db);
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

            const std::string sql(request.payload.begin(), request.payload.end());
            auto result = compiler.compile(sql);
            response_out.warnings = result.warnings();
            response_out.errors = result.errors();
            if (result.success())
            {
                response_out.bytecode = result.bytecode();
                response_out.success = true;
                return core::Status::OK;
            }
            return core::Status::INVALID_ARGUMENT;
        }

        auto compileMySql(core::Database *db,
                          const DialectCompilerRequest &request,
                          DialectCompilerResponse &response_out) -> core::Status
        {
            if (request.payload_format != DialectCompilerPayloadFormat::SQL_TEXT)
            {
                response_out.errors.push_back("MySQL compiler UDR only accepts SQL_TEXT payloads");
                return core::Status::NOT_SUPPORTED;
            }

            MySQLQueryCompiler compiler(db);
            if (!request.session.current_schema_name.empty())
            {
                compiler.setDefaultSchema(request.session.current_schema_name);
            }

            const std::string sql(request.payload.begin(), request.payload.end());
            auto result = compiler.compile(sql);
            response_out.warnings = result.warnings();
            response_out.errors = result.errors();
            if (result.success())
            {
                response_out.bytecode = result.bytecode();
                response_out.success = true;
                return core::Status::OK;
            }
            return core::Status::INVALID_ARGUMENT;
        }
    } // namespace

    auto builtinDialectCompilerInstallDescriptors()
        -> const std::vector<DialectCompilerInstallDescriptor> &
    {
        static const std::vector<DialectCompilerInstallDescriptor> descriptors =
            makeInstallDescriptors();
        return descriptors;
    }

    auto defaultDialectCompilerRegistry() -> udr::LanguageUdrRegistry &
    {
        static udr::LanguageUdrRegistry registry;
        static std::once_flag init_once;
        std::call_once(init_once, [&]() { seedRegistry(registry); });
        return registry;
    }

    auto compileDialectToSblr(core::Database *db,
                              const DialectCompilerRequest &request,
                              DialectCompilerResponse &response_out,
                              core::ErrorContext *ctx,
                              const udr::LanguageUdrRegistry *registry_override) -> core::Status
    {
        response_out = DialectCompilerResponse{};
        response_out.contract_id = kDialectCompilerContractId;

        DialectCompilerRequest normalized = request;
        if (isZeroUuid(normalized.request_id))
        {
            normalized.request_id = core::generateUuidV7();
        }
        normalized.session.profile_id =
            canonicalizeProfileId(normalized.session.profile_id, normalized.module_name);
        if (normalized.session.profile_version.empty())
        {
            normalized.session.profile_version = "1.0";
        }
        if (normalized.session.dialect_tag.empty())
        {
            normalized.session.dialect_tag = normalized.session.profile_id;
        }
        if (normalized.session.emulated_schema_root.empty())
        {
            normalized.session.emulated_schema_root = normalized.session.current_schema_name;
        }
        if (isZeroUuid(normalized.session.principal_id))
        {
            normalized.session.principal_id = core::generateUuidV7();
        }
        if (normalized.session.transaction_id == 0)
        {
            normalized.session.transaction_id = 1;
        }

        response_out.profile_id = normalized.session.profile_id;
        response_out.module_name = normalized.module_name;
        response_out.native_feature_key = featureKeyForRequest(normalized);

        if (normalized.payload.empty())
        {
            response_out.errors.push_back("Dialect compiler request payload is empty");
            return setError(ctx, core::Status::INVALID_ARGUMENT,
                            "Dialect compiler request payload is empty");
        }

        udr::LanguageUdrCompileRequest udr_request{};
        udr_request.request_id = normalized.request_id;
        udr_request.profile_id = normalized.session.profile_id;
        udr_request.profile_version = normalized.session.profile_version;
        udr_request.payload_format =
            normalized.payload_format == DialectCompilerPayloadFormat::FIREBIRD_BLR
                ? "FIREBIRD_BLR"
                : "SQL_TEXT";
        udr_request.payload = normalized.payload;
        udr_request.session_option_signature = buildSessionOptionSignature(normalized);
        udr_request.principal_id = normalized.session.principal_id;
        udr_request.role_context_signature = buildRoleContextSignature(normalized);
        udr_request.transaction_id = normalized.session.transaction_id;
        udr_request.native_feature_key = response_out.native_feature_key;
        udr_request.compile_permission_granted = normalized.compile_permission_granted;
        udr_request.requires_network_access = normalized.requires_network_access;
        udr_request.requires_filesystem_write = normalized.requires_filesystem_write;
        udr_request.sandbox_policy = normalized.sandbox_policy;
        udr_request.resource_limits = normalized.resource_limits;

        const auto &registry =
            registry_override != nullptr ? *registry_override : defaultDialectCompilerRegistry();
        const core::EmulationPackageManifest *package_manifest = nullptr;
        core::Status package_status = core::resolveInstalledEmulationPackage(
            normalized.session.profile_id,
            core::EmulationPackageKind::COMPILER_UDR,
            package_manifest,
            ctx);
        if (package_status != core::Status::OK)
        {
            response_out.errors.push_back(
                ctx != nullptr && !ctx->message.empty()
                    ? ctx->message
                    : std::string("Dialect compiler UDR package is not installed"));
            return package_status;
        }

        udr::LanguageUdrRegistration selected_module{};
        core::Status status =
            udr::LanguageUdrRuntimeBoundary::preflightCompile(
                registry, udr_request, selected_module, ctx);
        if (status != core::Status::OK)
        {
            response_out.errors.push_back(
                ctx != nullptr && !ctx->message.empty()
                    ? ctx->message
                    : std::string("Dialect compiler UDR preflight rejected request"));
            return status;
        }
        response_out.module_name = package_manifest != nullptr
                                       ? package_manifest->package_name
                                       : selected_module.module_name;

        if (normalized.session.profile_id == "firebirdsql")
        {
            status = compileFirebird(db, normalized, response_out);
        }
        else if (normalized.session.profile_id == "scratchbird")
        {
            status = compileScratchBird(db, normalized, response_out);
        }
        else if (normalized.session.profile_id == "postgresql")
        {
            status = compilePostgreSql(db, normalized, response_out);
        }
        else if (normalized.session.profile_id == "mysql")
        {
            status = compileMySql(db, normalized, response_out);
        }
        else
        {
            response_out.errors.push_back(
                "No installed dialect compiler UDR for profile '" +
                normalized.session.profile_id + "'");
            status = core::Status::NOT_FOUND;
        }

        if (status != core::Status::OK)
        {
            const std::string message =
                !response_out.errors.empty()
                    ? response_out.errors.front()
                    : "Dialect compiler UDR failed to produce SBLR";
            return setError(ctx, status, message);
        }

        return core::Status::OK;
    }

} // namespace scratchbird::sblr
