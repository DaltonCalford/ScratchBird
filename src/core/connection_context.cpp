/*
 * ScratchBird
 * Copyright (c) 2025-2026 Dalton Calford
 *
 * Licensed under the Initial Developer's Public License Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 * https://www.firebirdsql.org/en/initial-developer-s-public-license-version-1-0/
 */
#include "scratchbird/core/connection_context.h"
#include "scratchbird/core/database.h"
#include "scratchbird/core/table_stats_manager.h"
#include "scratchbird/core/proc_array.h"
#include "scratchbird/core/logger.h"
#include "scratchbird/core/lock_manager.h"
#include "scratchbird/core/catalog_manager.h"
#include "scratchbird/core/config.h"
#include "scratchbird/core/telemetry.h"
#include "scratchbird/core/storage_engine.h"
#include "scratchbird/core/mga_backout_engine.h"
#include "scratchbird/core/heap_page.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <limits>

namespace scratchbird::core
{
    using json = nlohmann::json;

    // Thread-local storage for current connection context
    thread_local ConnectionContext *ConnectionContext::current_ = nullptr;

    namespace {
        uint64_t nowMicros()
        {
            return std::chrono::duration_cast<std::chrono::microseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                .count();
        }

        uint64_t fnv1a64(const std::string& value)
        {
            uint64_t hash = 1469598103934665603ULL;
            for (unsigned char c : value)
            {
                hash ^= static_cast<uint64_t>(c);
                hash *= 1099511628211ULL;
            }
            return hash;
        }

        std::string normalizeSessionVar(const std::string& name)
        {
            std::string out;
            out.reserve(name.size());
            for (unsigned char c : name)
            {
                out.push_back(static_cast<char>(std::toupper(c)));
            }
            return out;
        }

        std::string normalizeSchemaPathForContext(const std::string& value)
        {
            std::string normalized;
            normalized.reserve(value.size());
            for (char ch : value)
            {
                normalized.push_back(ch == '/' ? '.' : ch);
            }
            return normalized;
        }

        std::vector<std::string> splitSchemaComponentsLocal(const std::string& value)
        {
            std::vector<std::string> components;
            std::string token;
            for (char ch : value)
            {
                if (ch == '.')
                {
                    if (!token.empty())
                    {
                        components.push_back(token);
                        token.clear();
                    }
                    continue;
                }
                token.push_back(ch);
            }
            if (!token.empty())
            {
                components.push_back(token);
            }
            return components;
        }

        bool isEmulatedDialect(const std::string& dialect_tag)
        {
            const std::string upper = IdentifierUtils::toUpper(dialect_tag);
            return upper == "MYSQL" || upper == "POSTGRESQL" || upper == "FIREBIRD";
        }

        bool isEmulatedSchemaPathForDialect(const std::string& schema_path,
                                            const std::string& dialect_tag)
        {
            const std::string normalized = normalizeSchemaPathForContext(schema_path);
            const std::vector<std::string> components = splitSchemaComponentsLocal(normalized);
            if (components.empty())
            {
                return false;
            }

            size_t start = 0;
            if (IdentifierUtils::namesMatch(components[0], false, "root", false))
            {
                start = 1;
            }
            if (components.size() <= start)
            {
                return false;
            }

            const std::string upper_dialect = IdentifierUtils::toUpper(dialect_tag);
            if (IdentifierUtils::namesMatch(components[start], false, "emulated", false) ||
                IdentifierUtils::namesMatch(components[start], false, "emulation", false))
            {
                return components.size() > (start + 1) &&
                       IdentifierUtils::namesMatch(components[start + 1],
                                                   false,
                                                   upper_dialect,
                                                   false);
            }
            if (IdentifierUtils::namesMatch(components[start], false, "remote", false))
            {
                return components.size() > (start + 2) &&
                       (IdentifierUtils::namesMatch(components[start + 1],
                                                    false,
                                                    "emulated",
                                                    false) ||
                        IdentifierUtils::namesMatch(components[start + 1],
                                                    false,
                                                    "emulation",
                                                    false)) &&
                       IdentifierUtils::namesMatch(components[start + 2],
                                                   false,
                                                   upper_dialect,
                                                   false);
            }
            return false;
        }

        std::string lastSchemaComponentLocal(const std::string& schema_path)
        {
            const std::string normalized = normalizeSchemaPathForContext(schema_path);
            const std::vector<std::string> components = splitSchemaComponentsLocal(normalized);
            return components.empty() ? std::string() : components.back();
        }

        bool parseUuidTextLocal(const std::string& text, ID& out)
        {
            std::string hex;
            hex.reserve(32);
            for (char c : text)
            {
                if (c != '-')
                {
                    hex.push_back(c);
                }
            }
            if (hex.size() != 32)
            {
                return false;
            }
            auto from_hex = [](char c) -> int {
                if (c >= '0' && c <= '9')
                {
                    return c - '0';
                }
                if (c >= 'a' && c <= 'f')
                {
                    return 10 + (c - 'a');
                }
                if (c >= 'A' && c <= 'F')
                {
                    return 10 + (c - 'A');
                }
                return -1;
            };
            for (size_t i = 0; i < out.bytes.size(); ++i)
            {
                int hi = from_hex(hex[i * 2]);
                int lo = from_hex(hex[i * 2 + 1]);
                if (hi < 0 || lo < 0)
                {
                    return false;
                }
                out.bytes[i] = static_cast<uint8_t>((hi << 4) | lo);
            }
            return true;
        }

        const char* objectTypeLabel(uint8_t type_code)
        {
            using OT = CatalogManager::ObjectType;
            switch (static_cast<OT>(type_code))
            {
                case OT::SCHEMA:
                    return "SCHEMA";
                case OT::TABLE:
                    return "TABLE";
                default:
                    return "OBJECT";
            }
        }

        std::string digestHex(uint64_t value)
        {
            char buf[17];
            std::snprintf(buf, sizeof(buf), "%016llX",
                          static_cast<unsigned long long>(value));
            return std::string(buf);
        }

        std::string normalizeDigestText(const std::string& sql)
        {
            std::string out;
            out.reserve(sql.size());
            bool in_quote = false;
            char quote_char = '\0';
            bool last_space = false;
            auto hasUnarySignContext = [&sql](size_t pos) -> bool {
                if (pos >= sql.size()) {
                    return false;
                }
                const char sign = sql[pos];
                if ((sign != '+' && sign != '-') ||
                    pos + 1 >= sql.size() ||
                    !std::isdigit(static_cast<unsigned char>(sql[pos + 1]))) {
                    return false;
                }

                // Unary sign is valid at start or after another operator/grouping token.
                size_t prev = pos;
                while (prev > 0 &&
                       std::isspace(static_cast<unsigned char>(sql[prev - 1]))) {
                    --prev;
                }
                if (prev == 0) {
                    return true;
                }

                const char prior = sql[prev - 1];
                switch (prior) {
                    case '(':
                    case '[':
                    case '{':
                    case ',':
                    case ':':
                    case '=':
                    case '<':
                    case '>':
                    case '!':
                    case '~':
                    case '+':
                    case '-':
                    case '*':
                    case '/':
                    case '%':
                    case '^':
                    case '&':
                    case '|':
                        return true;
                    default:
                        return false;
                }
            };

            for (size_t i = 0; i < sql.size(); ++i)
            {
                unsigned char c = static_cast<unsigned char>(sql[i]);
                if (in_quote)
                {
                    if (c == static_cast<unsigned char>(quote_char))
                    {
                        if (i + 1 < sql.size() && sql[i + 1] == quote_char)
                        {
                            ++i;
                            continue;
                        }
                        in_quote = false;
                    }
                    continue;
                }

                if (c == '\'' || c == '"')
                {
                    in_quote = true;
                    quote_char = static_cast<char>(c);
                    if (out.empty() || out.back() != '?')
                    {
                        out.push_back('?');
                    }
                    last_space = false;
                    continue;
                }

                if (std::isspace(c))
                {
                    if (!last_space && !out.empty())
                    {
                        out.push_back(' ');
                        last_space = true;
                    }
                    continue;
                }

                const bool is_signed_numeric = hasUnarySignContext(i);
                if (std::isdigit(c) || is_signed_numeric)
                {
                    size_t j = i;
                    if (is_signed_numeric)
                    {
                        ++j;
                    }
                    if (sql[j] == '0' && j + 1 < sql.size() &&
                        (sql[j + 1] == 'x' || sql[j + 1] == 'X'))
                    {
                        j += 2;
                        while (j < sql.size() &&
                               std::isxdigit(static_cast<unsigned char>(sql[j])))
                        {
                            ++j;
                        }
                    }
                    else
                    {
                        while (j < sql.size() &&
                               std::isdigit(static_cast<unsigned char>(sql[j])))
                        {
                            ++j;
                        }
                        if (j < sql.size() && sql[j] == '.')
                        {
                            ++j;
                            while (j < sql.size() &&
                                   std::isdigit(static_cast<unsigned char>(sql[j])))
                            {
                                ++j;
                            }
                        }
                        if (j < sql.size() && (sql[j] == 'e' || sql[j] == 'E'))
                        {
                            ++j;
                            if (j < sql.size() && (sql[j] == '+' || sql[j] == '-'))
                            {
                                ++j;
                            }
                            while (j < sql.size() &&
                                   std::isdigit(static_cast<unsigned char>(sql[j])))
                            {
                                ++j;
                            }
                        }
                    }

                    if (out.empty() || out.back() != '?')
                    {
                        out.push_back('?');
                    }
                    last_space = false;
                    i = j - 1;
                    continue;
                }

                char out_c = std::isalpha(c) ? static_cast<char>(std::toupper(c))
                                             : static_cast<char>(c);
                out.push_back(out_c);
                last_space = false;
            }

            if (!out.empty() && out.back() == ' ')
            {
                out.pop_back();
            }

            return out;
        }

        std::string baseNameFromPath(const std::string& path)
        {
            size_t last_slash = path.find_last_of("/\\");
            if (last_slash == std::string::npos)
            {
                return path;
            }
            return path.substr(last_slash + 1);
        }

        std::string classifyQueryType(const std::string& keyword)
        {
            if (keyword == "SELECT" || keyword == "WITH")
            {
                return "select";
            }
            if (keyword == "INSERT")
            {
                return "insert";
            }
            if (keyword == "UPDATE")
            {
                return "update";
            }
            if (keyword == "DELETE")
            {
                return "delete";
            }
            if (keyword == "MERGE")
            {
                return "merge";
            }
            if (keyword == "COPY")
            {
                return "copy";
            }
            if (keyword == "CREATE" || keyword == "ALTER" || keyword == "DROP" ||
                keyword == "TRUNCATE" || keyword == "COMMENT" || keyword == "GRANT" ||
                keyword == "REVOKE" || keyword == "RENAME")
            {
                return "ddl";
            }
            if (!keyword.empty())
            {
                return "other";
            }
            return "unknown";
        }

        bool isZeroUuidLocal(const ID& id)
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

        void appendJsonString(std::string& out, const std::string& value)
        {
            static const char kHexDigits[] = "0123456789abcdef";
            out.push_back('"');
            for (unsigned char c : value)
            {
                switch (c)
                {
                    case '\\': out.append("\\\\"); break;
                    case '"': out.append("\\\""); break;
                    case '\n': out.append("\\n"); break;
                    case '\r': out.append("\\r"); break;
                    case '\t': out.append("\\t"); break;
                    default:
                        if (c < 0x20)
                        {
                            out.append("\\u00");
                            out.push_back(kHexDigits[(c >> 4) & 0xF]);
                            out.push_back(kHexDigits[c & 0xF]);
                        }
                        else
                        {
                            out.push_back(static_cast<char>(c));
                        }
                        break;
                }
            }
            out.push_back('"');
        }

        CatalogManager::EmulationEngine catalogEmulationEngineForMode(const std::string& mode)
        {
            std::string folded;
            folded.reserve(mode.size());
            for (unsigned char c : mode)
            {
                folded.push_back(static_cast<char>(std::tolower(c)));
            }

            using Engine = CatalogManager::EmulationEngine;
            if (folded.empty() || folded == "native")
            {
                return Engine::NATIVE;
            }
            if (folded == "firebird" || folded == "firebirdsql")
            {
                return Engine::FIREBIRD;
            }
            if (folded == "postgresql")
            {
                return Engine::POSTGRESQL;
            }
            if (folded == "mysql")
            {
                return Engine::MYSQL;
            }
            if (folded == "cassandra")
            {
                return Engine::CASSANDRA;
            }
            if (folded == "milvus")
            {
                return Engine::MILVUS;
            }
            if (folded == "mongodb")
            {
                return Engine::MONGODB;
            }
            if (folded == "neo4j")
            {
                return Engine::NEO4J;
            }
            if (folded == "redis")
            {
                return Engine::REDIS;
            }
            if (folded == "mariadb")
            {
                return Engine::MARIADB;
            }
            if (folded == "influxdb")
            {
                return Engine::INFLUXDB;
            }
            if (folded == "clickhouse")
            {
                return Engine::CLICKHOUSE;
            }
            if (folded == "opensearch")
            {
                return Engine::OPENSEARCH;
            }
            if (folded == "duckdb")
            {
                return Engine::DUCKDB;
            }
            return Engine::NATIVE;
        }

        void appendJsonFieldPrefix(std::string& out, const char* key, bool& first)
        {
            if (!first)
            {
                out.push_back(',');
            }
            first = false;
            appendJsonString(out, key);
            out.push_back(':');
        }

        std::string buildTransactionBeginPayload(const Database* db, const ConnectionContext* conn)
        {
            std::string payload = "{";
            bool first = true;

            appendJsonFieldPrefix(payload, "database_uuid", first);
            appendJsonString(payload, db->uuid().toString());
            appendJsonFieldPrefix(payload, "node_uuid", first);
            appendJsonString(payload, db->server_instance_id().toString());
            appendJsonFieldPrefix(payload, "begin_time", first);
            payload += std::to_string(static_cast<uint64_t>(conn->getTransactionStartTime().count()));
            appendJsonFieldPrefix(payload, "transaction_outcome", first);
            appendJsonString(payload, "in_progress");
            appendJsonFieldPrefix(payload, "emulation_engine", first);
            appendJsonString(payload, conn->emulationMode());
            appendJsonFieldPrefix(payload, "isolation_level", first);
            payload += std::to_string(static_cast<uint32_t>(conn->getIsolationLevel()));
            appendJsonFieldPrefix(payload, "read_only", first);
            payload += conn->isReadOnly() ? "true" : "false";
            appendJsonFieldPrefix(payload, "autocommit", first);
            payload += (conn->autocommitMode() && !conn->autocommitSuspended()) ? "true" : "false";

            payload.push_back('}');
            return payload;
        }

        std::string buildTransactionContextPayload(const ConnectionContext* conn)
        {
            std::string payload = "{";
            bool first = true;

            appendJsonFieldPrefix(payload, "session_uuid", first);
            appendJsonString(payload, conn->effectiveSessionId().toString());
            appendJsonFieldPrefix(payload, "connection_uuid", first);
            appendJsonString(payload, conn->attachmentId().toString());
            appendJsonFieldPrefix(payload, "user_uuid", first);
            appendJsonString(payload, conn->getCurrentUserId().toString());
            appendJsonFieldPrefix(payload, "role_uuid", first);
            appendJsonString(payload, conn->getActiveRoleId().toString());
            appendJsonFieldPrefix(payload, "auth_context_uuid", first);
            appendJsonString(payload, conn->authKeyId().toString());

            payload.push_back('}');
            return payload;
        }

        std::string buildTransactionTerminalPayload(const Database* db,
                                                    const ConnectionContext* conn,
                                                    bool committed,
                                                    uint64_t start_time,
                                                    uint64_t end_time)
        {
            std::string payload = "{";
            bool first = true;

            appendJsonFieldPrefix(payload, "database_uuid", first);
            appendJsonString(payload, db->uuid().toString());
            appendJsonFieldPrefix(payload, "node_uuid", first);
            appendJsonString(payload, db->server_instance_id().toString());
            appendJsonFieldPrefix(payload, "session_uuid", first);
            appendJsonString(payload, conn->effectiveSessionId().toString());
            appendJsonFieldPrefix(payload, "connection_uuid", first);
            appendJsonString(payload, conn->attachmentId().toString());
            appendJsonFieldPrefix(payload, "user_uuid", first);
            appendJsonString(payload, conn->getCurrentUserId().toString());
            appendJsonFieldPrefix(payload, "role_uuid", first);
            appendJsonString(payload, conn->getActiveRoleId().toString());
            appendJsonFieldPrefix(payload, "begin_time", first);
            payload += std::to_string(start_time);
            appendJsonFieldPrefix(payload, committed ? "commit_time" : "rollback_time", first);
            payload += std::to_string(end_time);
            appendJsonFieldPrefix(payload, "transaction_outcome", first);
            appendJsonString(payload, committed ? "committed" : "aborted");
            appendJsonFieldPrefix(payload, "last_statement_hash", first);
            payload += std::to_string(conn->lastStatementHash());
            appendJsonFieldPrefix(payload, "last_statement_time", first);
            payload += std::to_string(conn->lastStatementTime());
            appendJsonFieldPrefix(payload, "last_error_code", first);
            payload += std::to_string(conn->lastErrorCode());
            appendJsonFieldPrefix(payload, "last_sqlstate", first);
            appendJsonString(payload, conn->lastSqlstate());

            payload.push_back('}');
            return payload;
        }

        constexpr uint64_t kForensicReplayRetentionMicros =
            7ULL * 24ULL * 60ULL * 60ULL * 1000000ULL;

        bool parseUint64Ascii(const std::string& text, uint64_t& value_out)
        {
            if (text.empty())
            {
                return false;
            }
            char* end = nullptr;
            errno = 0;
            const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
            if (errno != 0 || end != text.c_str() + text.size())
            {
                return false;
            }
            value_out = static_cast<uint64_t>(parsed);
            return true;
        }

        std::string getManifestValue(const std::string& manifest, const char* key)
        {
            const std::string prefix = std::string(key) + "=";
            size_t offset = 0;
            while (offset < manifest.size())
            {
                const size_t line_end = manifest.find('\n', offset);
                const size_t current_end =
                    (line_end == std::string::npos) ? manifest.size() : line_end;
                if (manifest.compare(offset, prefix.size(), prefix) == 0)
                {
                    return manifest.substr(offset + prefix.size(),
                                           current_end - offset - prefix.size());
                }
                if (line_end == std::string::npos)
                {
                    break;
                }
                offset = line_end + 1;
            }
            return {};
        }

        std::string buildActiveTxManifest(
            const TransactionSnapshot& snapshot)
        {
            std::string manifest = "active_txids=";
            for (size_t i = 0; i < snapshot.active_txid_set.size(); ++i)
            {
                if (i != 0)
                {
                    manifest.push_back(',');
                }
                manifest += std::to_string(snapshot.active_txid_set[i]);
            }
            manifest.push_back('\n');
            return manifest;
        }

        std::string buildVisibilityManifest(
            const TransactionSnapshot& snapshot,
            IsolationLevel isolation_level)
        {
            std::string manifest;
            manifest += "snapshot_txid_low=";
            manifest += std::to_string(snapshot.snapshot_txid_low);
            manifest.push_back('\n');
            manifest += "snapshot_txid_high=";
            manifest += std::to_string(snapshot.snapshot_txid_high);
            manifest.push_back('\n');
            manifest += "snapshot_commit_seqno_high=";
            manifest += std::to_string(snapshot.snapshot_commit_seqno_high);
            manifest.push_back('\n');
            manifest += "snapshot_serial=";
            manifest += std::to_string(snapshot.snapshot_serial);
            manifest.push_back('\n');
            manifest += "capture_time=";
            manifest += std::to_string(snapshot.capture_time);
            manifest.push_back('\n');
            manifest += "isolation_level=";
            manifest += std::to_string(static_cast<uint32_t>(isolation_level));
            manifest.push_back('\n');
            return manifest;
        }

        Status parseForensicReplaySnapshot(const std::string& active_manifest,
                                           const std::string& visibility_manifest,
                                           TransactionSnapshot& snapshot_out,
                                           ErrorContext* ctx)
        {
            snapshot_out = TransactionSnapshot{};
            const std::string txid_low_text =
                getManifestValue(visibility_manifest, "snapshot_txid_low");
            if (!txid_low_text.empty() &&
                !parseUint64Ascii(txid_low_text, snapshot_out.snapshot_txid_low))
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::DATA_CORRUPTED,
                                  "forensic visibility manifest has invalid snapshot_txid_low");
                return Status::DATA_CORRUPTED;
            }

            const std::string txid_high_text =
                getManifestValue(visibility_manifest, "snapshot_txid_high");
            if (!parseUint64Ascii(txid_high_text, snapshot_out.snapshot_txid_high))
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::DATA_CORRUPTED,
                                  "forensic visibility manifest missing snapshot_txid_high");
                return Status::DATA_CORRUPTED;
            }

            const std::string snapshot_serial_text =
                getManifestValue(visibility_manifest, "snapshot_serial");
            if (!snapshot_serial_text.empty() &&
                !parseUint64Ascii(snapshot_serial_text, snapshot_out.snapshot_serial))
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::DATA_CORRUPTED,
                                  "forensic visibility manifest has invalid snapshot_serial");
                return Status::DATA_CORRUPTED;
            }

            const std::string capture_time_text =
                getManifestValue(visibility_manifest, "capture_time");
            if (!capture_time_text.empty() &&
                !parseUint64Ascii(capture_time_text, snapshot_out.capture_time))
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::DATA_CORRUPTED,
                                  "forensic visibility manifest has invalid capture_time");
                return Status::DATA_CORRUPTED;
            }

            const std::string commit_seqno_text =
                getManifestValue(visibility_manifest, "snapshot_commit_seqno_high");
            if (!commit_seqno_text.empty() &&
                !parseUint64Ascii(commit_seqno_text, snapshot_out.snapshot_commit_seqno_high))
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::DATA_CORRUPTED,
                                  "forensic visibility manifest has invalid snapshot_commit_seqno_high");
                return Status::DATA_CORRUPTED;
            }

            const std::string active_list = getManifestValue(active_manifest, "active_txids");
            snapshot_out.active_txid_set.clear();
            size_t offset = 0;
            while (offset < active_list.size())
            {
                const size_t comma = active_list.find(',', offset);
                const size_t token_end =
                    (comma == std::string::npos) ? active_list.size() : comma;
                const std::string token = active_list.substr(offset, token_end - offset);
                if (!token.empty())
                {
                    uint64_t value = 0;
                    if (!parseUint64Ascii(token, value))
                    {
                        SET_ERROR_CONTEXT(ctx,
                                          Status::DATA_CORRUPTED,
                                          "forensic active transaction manifest is invalid");
                        return Status::DATA_CORRUPTED;
                    }
                    snapshot_out.active_txid_set.push_back(value);
                }
                if (comma == std::string::npos)
                {
                    break;
                }
                offset = comma + 1;
            }

            std::sort(snapshot_out.active_txid_set.begin(), snapshot_out.active_txid_set.end());
            snapshot_out.active_txid_set.erase(
                std::unique(snapshot_out.active_txid_set.begin(),
                            snapshot_out.active_txid_set.end()),
                snapshot_out.active_txid_set.end());
            if (snapshot_out.snapshot_txid_low == 0)
            {
                snapshot_out.snapshot_txid_low = snapshot_out.snapshot_txid_high;
                if (!snapshot_out.active_txid_set.empty())
                {
                    snapshot_out.snapshot_txid_low = snapshot_out.active_txid_set.front();
                }
            }
            return Status::OK;
        }
    }

    ConnectionContext::ConnectionContext(Database *db, uint32_t proc_id)
        : db_(db), txn_manager_(db ? db->transaction_manager() : nullptr), proc_id_(proc_id),
          current_xid_(0) // Will be set by initialize()
          ,
          current_transaction_uuid_(),
          xact_start_time_(std::chrono::microseconds(0)),
          lineage_root_event_id_(),
          transaction_start_oit_(0),
          transaction_start_oat_(0),
          transaction_start_ost_(0),
          last_commit_seqno_(0),
          current_schema_epoch_uuid_(),
          transaction_start_schema_epoch_uuid_(),
          retained_transaction_snapshot_(nullptr),
          statement_transaction_snapshot_(nullptr),
          forensic_replay_binding_(nullptr),
          current_user_id_(), // Zero UUID - will be set during authentication
          active_role_id_(),  // Zero UUID - no role active initially
          is_superuser_(false), // Will be set during authentication
          session_user_id_(),   // WP-5 EXEC-M3: Zero UUID - set on first setCurrentUser() call
          session_is_superuser_(false),  // WP-5 EXEC-M3: Set on first setCurrentUser() call
          isolation_level_(IsolationLevel::SNAPSHOT), // Default to SNAPSHOT
          read_committed_mode_(ReadCommittedMode::READ_CONSISTENCY)
          ,
          is_read_only_(db != nullptr && db->startup_quarantine_active()),
          wait_for_locks_(true) // Default: wait for locks
          ,
          lock_timeout_seconds_(config::DEFAULT_LOCK_TIMEOUT_SECONDS) // Default: 60 second timeout
          ,
          autocommit_mode_(false),
          autocommit_suspended_(false),
          attachment_id_(generateUuidV7()),
          protocol_session_id_(),
          session_id_(),
          authkey_id_(),
          emulation_mode_("native"),
          policy_epoch_global_(0),
          policy_epoch_table_(0),
          security_context_initialized_(false),
          security_context_staged_(false),
          pending_user_change_(false),
          pending_role_change_(false),
          pending_session_change_(false),
          pending_user_id_(),
          pending_role_id_(),
          pending_is_superuser_(false),
          pending_session_id_(),
          pending_authkey_id_(),
          pending_emulation_mode_(),
          pending_policy_epoch_global_(0),
          pending_policy_epoch_table_(0),
          default_isolation_level_(IsolationLevel::SNAPSHOT),
          default_read_committed_mode_(ReadCommittedMode::READ_CONSISTENCY),
          default_is_read_only_(db != nullptr && db->startup_quarantine_active()),
          default_wait_for_locks_(true),
          default_lock_timeout_seconds_(config::DEFAULT_LOCK_TIMEOUT_SECONDS),
          settings_staged_(false), next_isolation_level_(IsolationLevel::SNAPSHOT),
          next_read_committed_mode_(ReadCommittedMode::READ_CONSISTENCY),
          next_is_read_only_(db != nullptr && db->startup_quarantine_active()),
          next_wait_for_locks_(true),
          next_lock_timeout_seconds_(config::DEFAULT_LOCK_TIMEOUT_SECONDS),
          statement_xid_(0)
    {
        assert(db != nullptr && "Database must not be null");
        assert(txn_manager_ != nullptr && "TransactionManager must not be null");

        // Initialize UUIDs to zero (no user authenticated yet)
        std::memset(&current_user_id_, 0, sizeof(current_user_id_));
        std::memset(&active_role_id_, 0, sizeof(active_role_id_));
        std::memset(&current_transaction_uuid_, 0, sizeof(current_transaction_uuid_));
        std::memset(&lineage_root_event_id_, 0, sizeof(lineage_root_event_id_));
        std::memset(&current_schema_epoch_uuid_, 0, sizeof(current_schema_epoch_uuid_));
        std::memset(&transaction_start_schema_epoch_uuid_, 0, sizeof(transaction_start_schema_epoch_uuid_));
        std::memset(&current_schema_id_, 0, sizeof(current_schema_id_));
        std::memset(&session_user_id_, 0, sizeof(session_user_id_));  // WP-5 EXEC-M3
        std::memset(&protocol_session_id_, 0, sizeof(protocol_session_id_));
        std::memset(&session_id_, 0, sizeof(session_id_));
        std::memset(&authkey_id_, 0, sizeof(authkey_id_));
        std::memset(&pending_user_id_, 0, sizeof(pending_user_id_));
        std::memset(&pending_role_id_, 0, sizeof(pending_role_id_));
        std::memset(&pending_session_id_, 0, sizeof(pending_session_id_));
        std::memset(&pending_authkey_id_, 0, sizeof(pending_authkey_id_));
        // Note: current_schema_id_ will be set to PUBLIC schema during initialize()
    }

    ConnectionContext::~ConnectionContext()
    {
        // If we're still the current context, clear it
        if (current_ == this)
        {
            current_ = nullptr;
        }

        if (db_)
        {
            db_->unregisterConnectionContext(this);
        }

        // Rollback any outstanding transaction
        if (current_xid_ != 0)
        {
            ErrorContext err_ctx;
            // Final shutdown should not start a new transaction; end in-place to avoid orphaned XIDs.
            shutdownTransaction(&err_ctx);
        }

        {
            ErrorContext err_ctx;
            cleanupTempTablesOnSessionEnd(&err_ctx);
        }

        // Unregister this backend from ProcArray to free the slot
        if (proc_id_ != UINT32_MAX)
        {
            ProcArrayManager::unregisterBackend(proc_id_, nullptr);
            proc_id_ = UINT32_MAX;
        }
    }

    ConnectionContext::ConnectionContext(ConnectionContext &&other) noexcept
        : db_(other.db_), txn_manager_(other.txn_manager_), proc_id_(other.proc_id_),
          current_xid_(other.current_xid_), current_transaction_uuid_(other.current_transaction_uuid_),
          xact_start_time_(other.xact_start_time_),
          lineage_root_event_id_(other.lineage_root_event_id_),
          transaction_start_oit_(other.transaction_start_oit_),
          transaction_start_oat_(other.transaction_start_oat_),
          transaction_start_ost_(other.transaction_start_ost_),
          current_schema_epoch_uuid_(other.current_schema_epoch_uuid_),
          transaction_start_schema_epoch_uuid_(other.transaction_start_schema_epoch_uuid_),
          retained_transaction_snapshot_(std::move(other.retained_transaction_snapshot_)),
          statement_transaction_snapshot_(std::move(other.statement_transaction_snapshot_)),
          forensic_replay_binding_(std::move(other.forensic_replay_binding_)),
          current_user_id_(other.current_user_id_), active_role_id_(other.active_role_id_),
          is_superuser_(other.is_superuser_),
          session_user_id_(other.session_user_id_),  // WP-5 EXEC-M3
          session_is_superuser_(other.session_is_superuser_),  // WP-5 EXEC-M3
          current_schema_id_(other.current_schema_id_),
          current_schema_name_(std::move(other.current_schema_name_)),
          search_path_(std::move(other.search_path_)),
          dialect_tag_(std::move(other.dialect_tag_)),
          security_stack_(std::move(other.security_stack_)),
          isolation_level_(other.isolation_level_),
          read_committed_mode_(other.read_committed_mode_),
          is_read_only_(other.is_read_only_),
          wait_for_locks_(other.wait_for_locks_),
          lock_timeout_seconds_(other.lock_timeout_seconds_),
          autocommit_mode_(other.autocommit_mode_),
          autocommit_suspended_(other.autocommit_suspended_),
          role_switch_policy_(other.role_switch_policy_),
          attachment_id_(other.attachment_id_),
          protocol_session_id_(other.protocol_session_id_),
          session_id_(other.session_id_),
          authkey_id_(other.authkey_id_),
          emulation_mode_(std::move(other.emulation_mode_)),
          policy_epoch_global_(other.policy_epoch_global_),
          policy_epoch_table_(other.policy_epoch_table_),
          security_context_initialized_(other.security_context_initialized_),
          security_context_staged_(other.security_context_staged_),
          pending_user_change_(other.pending_user_change_),
          pending_role_change_(other.pending_role_change_),
          pending_session_change_(other.pending_session_change_),
          pending_user_id_(other.pending_user_id_),
          pending_role_id_(other.pending_role_id_),
          pending_is_superuser_(other.pending_is_superuser_),
          pending_session_id_(other.pending_session_id_),
          pending_authkey_id_(other.pending_authkey_id_),
          pending_emulation_mode_(std::move(other.pending_emulation_mode_)),
          pending_policy_epoch_global_(other.pending_policy_epoch_global_),
          pending_policy_epoch_table_(other.pending_policy_epoch_table_),
          sql_dialect_(other.sql_dialect_),
          charset_(std::move(other.charset_)),
          statement_timeout_seconds_(other.statement_timeout_seconds_),
          default_statement_timeout_seconds_(other.default_statement_timeout_seconds_),
          statement_timeout_local_override_(other.statement_timeout_local_override_),
          last_statement_text_(std::move(other.last_statement_text_)),
          last_statement_hash_(other.last_statement_hash_),
          last_statement_query_type_(std::move(other.last_statement_query_type_)),
          last_statement_type_(other.last_statement_type_),
          last_statement_status_(other.last_statement_status_),
          last_statement_time_(other.last_statement_time_),
          last_rows_affected_(other.last_rows_affected_),
          last_error_code_(other.last_error_code_),
          last_sqlstate_(std::move(other.last_sqlstate_)),
          last_activity_time_(other.last_activity_time_),
          connection_io_stats_(other.connection_io_stats_),
          transaction_io_stats_(other.transaction_io_stats_),
          statement_io_stats_(other.statement_io_stats_),
          statement_io_active_(other.statement_io_active_),
          statement_id_(other.statement_id_),
          pending_table_deltas_(std::move(other.pending_table_deltas_)),
          pending_transactional_ddl_batches_(std::move(other.pending_transactional_ddl_batches_)),
          default_isolation_level_(other.default_isolation_level_),
          default_read_committed_mode_(other.default_read_committed_mode_),
          default_is_read_only_(other.default_is_read_only_),
          default_wait_for_locks_(other.default_wait_for_locks_),
          default_lock_timeout_seconds_(other.default_lock_timeout_seconds_),
          settings_staged_(other.settings_staged_),
          next_isolation_level_(other.next_isolation_level_),
          next_read_committed_mode_(other.next_read_committed_mode_),
          next_is_read_only_(other.next_is_read_only_),
          next_wait_for_locks_(other.next_wait_for_locks_),
          next_lock_timeout_seconds_(other.next_lock_timeout_seconds_),
          statement_xid_(other.statement_xid_),
          last_statement_restart_decision_(other.last_statement_restart_decision_),
          statement_restart_count_(other.statement_restart_count_),
          table_reservations_(std::move(other.table_reservations_))
    {
        if (db_)
        {
            db_->rebindConnectionContext(proc_id_, this);
        }

        // Clear other's state - critical to invalidate proc_id_ so destructor doesn't unregister
        other.db_ = nullptr;
        other.txn_manager_ = nullptr;
        other.proc_id_ = UINT32_MAX;  // Invalidate so destructor doesn't double-unregister
        other.current_xid_ = 0;
        other.statement_xid_ = 0;
        other.last_statement_restart_decision_ = StatementRestartDecision{};
        other.statement_restart_count_ = 0;
        std::memset(&other.current_user_id_, 0, sizeof(other.current_user_id_));
        std::memset(&other.active_role_id_, 0, sizeof(other.active_role_id_));
        std::memset(&other.current_transaction_uuid_, 0, sizeof(other.current_transaction_uuid_));
        std::memset(&other.lineage_root_event_id_, 0, sizeof(other.lineage_root_event_id_));
        std::memset(&other.current_schema_epoch_uuid_, 0, sizeof(other.current_schema_epoch_uuid_));
        std::memset(&other.transaction_start_schema_epoch_uuid_, 0, sizeof(other.transaction_start_schema_epoch_uuid_));
        std::memset(&other.current_schema_id_, 0, sizeof(other.current_schema_id_));
        std::memset(&other.session_user_id_, 0, sizeof(other.session_user_id_));  // WP-5 EXEC-M3
        std::memset(&other.attachment_id_, 0, sizeof(other.attachment_id_));
        std::memset(&other.protocol_session_id_, 0, sizeof(other.protocol_session_id_));
        std::memset(&other.session_id_, 0, sizeof(other.session_id_));
        std::memset(&other.authkey_id_, 0, sizeof(other.authkey_id_));
        std::memset(&other.pending_user_id_, 0, sizeof(other.pending_user_id_));
        std::memset(&other.pending_role_id_, 0, sizeof(other.pending_role_id_));
        std::memset(&other.pending_session_id_, 0, sizeof(other.pending_session_id_));
        std::memset(&other.pending_authkey_id_, 0, sizeof(other.pending_authkey_id_));
        other.emulation_mode_.clear();
        other.policy_epoch_global_ = 0;
        other.policy_epoch_table_ = 0;
        other.security_context_initialized_ = false;
        other.security_context_staged_ = false;
        other.pending_user_change_ = false;
        other.pending_role_change_ = false;
        other.pending_session_change_ = false;
        other.pending_is_superuser_ = false;
        other.pending_emulation_mode_.clear();
        other.pending_policy_epoch_global_ = 0;
        other.pending_policy_epoch_table_ = 0;
        other.is_superuser_ = false;
        other.session_is_superuser_ = false;  // WP-5 EXEC-M3
        other.last_statement_text_.clear();
        other.last_statement_hash_ = 0;
        other.last_statement_query_type_.clear();
        other.last_statement_type_ = StatementType::UNKNOWN;
        other.last_statement_status_ = StatementStatus::UNKNOWN;
        other.last_statement_time_ = 0;
        other.last_rows_affected_ = 0;
        other.last_error_code_ = 0;
        other.last_sqlstate_.clear();
        other.last_activity_time_ = 0;
        other.statement_timeout_seconds_ = 0;
        other.default_statement_timeout_seconds_ = 0;
        other.statement_timeout_local_override_ = false;
        other.connection_io_stats_.reset();
        other.transaction_io_stats_.reset();
        other.statement_io_stats_.reset();
        other.statement_io_active_ = false;
        other.statement_id_ = 0;
        other.pending_table_deltas_.clear();
        other.pending_transactional_ddl_batches_.clear();
        other.role_switch_policy_ = RoleSwitchPolicy::REJECT;
    }

    ConnectionContext &ConnectionContext::operator=(ConnectionContext &&other) noexcept
    {
        if (this != &other)
        {
            // Cleanup current transaction if any
            if (current_xid_ != 0)
            {
                ErrorContext err_ctx;
                // Avoid starting a new transaction while being overwritten.
                shutdownTransaction(&err_ctx);
            }

            if (db_)
            {
                db_->unregisterConnectionContext(this);
            }

            // Unregister our current backend before taking the new one
            if (proc_id_ != UINT32_MAX)
            {
                ProcArrayManager::unregisterBackend(proc_id_, nullptr);
            }

            // Move state
            db_ = other.db_;
            txn_manager_ = other.txn_manager_;
            proc_id_ = other.proc_id_;
            current_xid_ = other.current_xid_;
            current_transaction_uuid_ = other.current_transaction_uuid_;
            xact_start_time_ = other.xact_start_time_;
            lineage_root_event_id_ = other.lineage_root_event_id_;
            transaction_start_oit_ = other.transaction_start_oit_;
            transaction_start_oat_ = other.transaction_start_oat_;
            transaction_start_ost_ = other.transaction_start_ost_;
            current_schema_epoch_uuid_ = other.current_schema_epoch_uuid_;
            transaction_start_schema_epoch_uuid_ = other.transaction_start_schema_epoch_uuid_;
            retained_transaction_snapshot_ = std::move(other.retained_transaction_snapshot_);
            statement_transaction_snapshot_ = std::move(other.statement_transaction_snapshot_);
            forensic_replay_binding_ = std::move(other.forensic_replay_binding_);
            current_user_id_ = other.current_user_id_;
            active_role_id_ = other.active_role_id_;
            is_superuser_ = other.is_superuser_;
            session_user_id_ = other.session_user_id_;  // WP-5 EXEC-M3
            session_is_superuser_ = other.session_is_superuser_;  // WP-5 EXEC-M3
            current_schema_id_ = other.current_schema_id_;
            current_schema_name_ = std::move(other.current_schema_name_);
            search_path_ = std::move(other.search_path_);
            dialect_tag_ = std::move(other.dialect_tag_);
            security_stack_ = std::move(other.security_stack_);
            isolation_level_ = other.isolation_level_;
            read_committed_mode_ = other.read_committed_mode_;
            is_read_only_ = other.is_read_only_;
            wait_for_locks_ = other.wait_for_locks_;
            lock_timeout_seconds_ = other.lock_timeout_seconds_;
            autocommit_mode_ = other.autocommit_mode_;
            autocommit_suspended_ = other.autocommit_suspended_;
            role_switch_policy_ = other.role_switch_policy_;
            attachment_id_ = other.attachment_id_;
            protocol_session_id_ = other.protocol_session_id_;
            session_id_ = other.session_id_;
            authkey_id_ = other.authkey_id_;
            emulation_mode_ = std::move(other.emulation_mode_);
            policy_epoch_global_ = other.policy_epoch_global_;
            policy_epoch_table_ = other.policy_epoch_table_;
            security_context_initialized_ = other.security_context_initialized_;
            security_context_staged_ = other.security_context_staged_;
            pending_user_change_ = other.pending_user_change_;
            pending_role_change_ = other.pending_role_change_;
            pending_session_change_ = other.pending_session_change_;
            pending_user_id_ = other.pending_user_id_;
            pending_role_id_ = other.pending_role_id_;
            pending_is_superuser_ = other.pending_is_superuser_;
            pending_session_id_ = other.pending_session_id_;
            pending_authkey_id_ = other.pending_authkey_id_;
            pending_emulation_mode_ = std::move(other.pending_emulation_mode_);
            pending_policy_epoch_global_ = other.pending_policy_epoch_global_;
            pending_policy_epoch_table_ = other.pending_policy_epoch_table_;
            sql_dialect_ = other.sql_dialect_;
            charset_ = std::move(other.charset_);
            statement_timeout_seconds_ = other.statement_timeout_seconds_;
            default_statement_timeout_seconds_ = other.default_statement_timeout_seconds_;
            statement_timeout_local_override_ = other.statement_timeout_local_override_;
            last_statement_text_ = std::move(other.last_statement_text_);
            last_statement_hash_ = other.last_statement_hash_;
            last_statement_query_type_ = std::move(other.last_statement_query_type_);
            last_statement_type_ = other.last_statement_type_;
            last_statement_status_ = other.last_statement_status_;
            last_statement_time_ = other.last_statement_time_;
            last_rows_affected_ = other.last_rows_affected_;
            last_error_code_ = other.last_error_code_;
            last_sqlstate_ = std::move(other.last_sqlstate_);
            last_activity_time_ = other.last_activity_time_;
            connection_io_stats_ = other.connection_io_stats_;
            transaction_io_stats_ = other.transaction_io_stats_;
            statement_io_stats_ = other.statement_io_stats_;
            statement_io_active_ = other.statement_io_active_;
            statement_id_ = other.statement_id_;
            pending_table_deltas_ = std::move(other.pending_table_deltas_);
            pending_transactional_ddl_batches_ =
                std::move(other.pending_transactional_ddl_batches_);
            default_isolation_level_ = other.default_isolation_level_;
            default_read_committed_mode_ = other.default_read_committed_mode_;
            default_is_read_only_ = other.default_is_read_only_;
            default_wait_for_locks_ = other.default_wait_for_locks_;
            default_lock_timeout_seconds_ = other.default_lock_timeout_seconds_;
            settings_staged_ = other.settings_staged_;
            next_isolation_level_ = other.next_isolation_level_;
            next_read_committed_mode_ = other.next_read_committed_mode_;
            next_is_read_only_ = other.next_is_read_only_;
            next_wait_for_locks_ = other.next_wait_for_locks_;
            next_lock_timeout_seconds_ = other.next_lock_timeout_seconds_;
            statement_xid_ = other.statement_xid_;
            last_statement_restart_decision_ = other.last_statement_restart_decision_;
            statement_restart_count_ = other.statement_restart_count_;
            table_reservations_ = std::move(other.table_reservations_);

            if (db_)
            {
                db_->rebindConnectionContext(proc_id_, this);
            }

            // Clear other's state - critical to invalidate proc_id_
            other.db_ = nullptr;
            other.txn_manager_ = nullptr;
            other.proc_id_ = UINT32_MAX;  // Invalidate so destructor doesn't double-unregister
            other.current_xid_ = 0;
            other.statement_xid_ = 0;
            other.last_statement_restart_decision_ = StatementRestartDecision{};
            other.statement_restart_count_ = 0;
            other.transaction_start_oit_ = 0;
            other.transaction_start_oat_ = 0;
            other.transaction_start_ost_ = 0;
            std::memset(&other.current_user_id_, 0, sizeof(other.current_user_id_));
            std::memset(&other.active_role_id_, 0, sizeof(other.active_role_id_));
            std::memset(&other.current_transaction_uuid_, 0, sizeof(other.current_transaction_uuid_));
            std::memset(&other.lineage_root_event_id_, 0, sizeof(other.lineage_root_event_id_));
            std::memset(&other.current_schema_epoch_uuid_, 0, sizeof(other.current_schema_epoch_uuid_));
            std::memset(&other.transaction_start_schema_epoch_uuid_, 0, sizeof(other.transaction_start_schema_epoch_uuid_));
            std::memset(&other.current_schema_id_, 0, sizeof(other.current_schema_id_));
            std::memset(&other.session_user_id_, 0, sizeof(other.session_user_id_));  // WP-5 EXEC-M3
            std::memset(&other.attachment_id_, 0, sizeof(other.attachment_id_));
            std::memset(&other.protocol_session_id_, 0, sizeof(other.protocol_session_id_));
            std::memset(&other.session_id_, 0, sizeof(other.session_id_));
            std::memset(&other.authkey_id_, 0, sizeof(other.authkey_id_));
            std::memset(&other.pending_user_id_, 0, sizeof(other.pending_user_id_));
            std::memset(&other.pending_role_id_, 0, sizeof(other.pending_role_id_));
            std::memset(&other.pending_session_id_, 0, sizeof(other.pending_session_id_));
            std::memset(&other.pending_authkey_id_, 0, sizeof(other.pending_authkey_id_));
            other.emulation_mode_.clear();
            other.policy_epoch_global_ = 0;
            other.policy_epoch_table_ = 0;
            other.security_context_initialized_ = false;
            other.security_context_staged_ = false;
            other.pending_user_change_ = false;
            other.pending_role_change_ = false;
            other.pending_session_change_ = false;
            other.pending_is_superuser_ = false;
            other.pending_emulation_mode_.clear();
            other.pending_policy_epoch_global_ = 0;
            other.pending_policy_epoch_table_ = 0;
            other.is_superuser_ = false;
            other.session_is_superuser_ = false;  // WP-5 EXEC-M3
            other.last_statement_text_.clear();
            other.last_statement_hash_ = 0;
            other.last_statement_query_type_.clear();
            other.last_statement_type_ = StatementType::UNKNOWN;
            other.last_statement_status_ = StatementStatus::UNKNOWN;
            other.last_statement_time_ = 0;
            other.last_rows_affected_ = 0;
            other.last_error_code_ = 0;
            other.last_sqlstate_.clear();
            other.last_activity_time_ = 0;
            other.statement_timeout_seconds_ = 0;
            other.default_statement_timeout_seconds_ = 0;
            other.statement_timeout_local_override_ = false;
            other.connection_io_stats_.reset();
            other.transaction_io_stats_.reset();
            other.statement_io_stats_.reset();
            other.statement_io_active_ = false;
            other.statement_id_ = 0;
            other.pending_table_deltas_.clear();
            other.pending_transactional_ddl_batches_.clear();
            other.role_switch_policy_ = RoleSwitchPolicy::REJECT;
        }
        return *this;
    }

    void ConnectionContext::set_current_schema(const std::string& schema)
    {
        current_schema_name_ = schema;
        if (schema.empty() || !isEmulatedDialect(dialect_tag_))
        {
            return;
        }

        if (isEmulatedSchemaPathForDialect(schema, dialect_tag_))
        {
            current_schema_name_ = normalizeSchemaPathForContext(schema);
            return;
        }

        const std::string schema_leaf = lastSchemaComponentLocal(schema);
        if (schema_leaf.empty())
        {
            return;
        }

        for (const auto& entry : search_path_)
        {
            if (!isEmulatedSchemaPathForDialect(entry, dialect_tag_))
            {
                continue;
            }
            if (IdentifierUtils::namesMatch(lastSchemaComponentLocal(entry),
                                            false,
                                            schema_leaf,
                                            false))
            {
                current_schema_name_ = normalizeSchemaPathForContext(entry);
                return;
            }
        }
    }

    void ConnectionContext::set_search_path(const std::vector<std::string>& paths)
    {
        search_path_ = paths;
        if (search_path_.empty() || !isEmulatedDialect(dialect_tag_))
        {
            return;
        }

        for (auto& entry : search_path_)
        {
            if (isEmulatedSchemaPathForDialect(entry, dialect_tag_))
            {
                entry = normalizeSchemaPathForContext(entry);
            }
        }

        if (current_schema_name_.empty())
        {
            return;
        }

        if (isEmulatedSchemaPathForDialect(current_schema_name_, dialect_tag_))
        {
            current_schema_name_ = normalizeSchemaPathForContext(current_schema_name_);
            return;
        }

        const std::string schema_leaf = lastSchemaComponentLocal(current_schema_name_);
        if (schema_leaf.empty())
        {
            return;
        }

        for (const auto& entry : search_path_)
        {
            if (!isEmulatedSchemaPathForDialect(entry, dialect_tag_))
            {
                continue;
            }
            if (IdentifierUtils::namesMatch(lastSchemaComponentLocal(entry),
                                            false,
                                            schema_leaf,
                                            false))
            {
                current_schema_name_ = entry;
                return;
            }
        }
    }

    ConnectionContext *ConnectionContext::getCurrent()
    {
        return current_;
    }

    void ConnectionContext::setCurrent(ConnectionContext *ctx)
    {
        current_ = ctx;
    }

    int32_t ConnectionContext::getCurrentProcId()
    {
        ConnectionContext *ctx = getCurrent();
        if (ctx == nullptr)
        {
            return -1; // No connection context
        }
        return static_cast<int32_t>(ctx->proc_id_);
    }

    uint64_t ConnectionContext::getCurrentTransactionId()
    {
        ConnectionContext *ctx = getCurrent();
        if (ctx == nullptr)
        {
            return 0; // Invalid XID
        }
        return ctx->current_xid_;
    }

    bool ConnectionContext::isForensicReplayActive() const
    {
        return forensic_replay_binding_ != nullptr;
    }

    ConnectionContext::ForensicReplayStatus ConnectionContext::getForensicReplayStatus() const
    {
        ForensicReplayStatus status{};
        if (!forensic_replay_binding_)
        {
            return status;
        }
        status.capsule_uuid = forensic_replay_binding_->capsule_uuid;
        status.resolved_tx_uuid = forensic_replay_binding_->tx_uuid;
        status.resolved_txid = forensic_replay_binding_->txid;
        status.schema_epoch_uuid = forensic_replay_binding_->schema_epoch_uuid;
        status.snapshot_kind = forensic_replay_binding_->snapshot_kind;
        status.is_active = true;
        return status;
    }

    const TransactionSnapshot*
    ConnectionContext::getForensicReplaySnapshot() const
    {
        return forensic_replay_binding_ ? &forensic_replay_binding_->snapshot : nullptr;
    }

    const TransactionSnapshot*
    ConnectionContext::getRetainedTransactionSnapshot() const
    {
        return retained_transaction_snapshot_.get();
    }

    const TransactionSnapshot*
    ConnectionContext::getStatementTransactionSnapshot() const
    {
        return statement_transaction_snapshot_.get();
    }

    auto ConnectionContext::resolveReadConsistencyVisibilityContext() const
        -> VisibilityContext
    {
        if (db_ != nullptr && db_->transaction_manager() != nullptr)
        {
            const auto resolved =
                db_->transaction_manager()->resolveVisibilityContext(current_xid_, this);
            VisibilityContext context{};
            context.valid = resolved.valid;
            context.mode = resolved.mode;
            context.reason = resolved.reason;
            context.reader_xid = resolved.reader_xid;
            context.snapshot = resolved.snapshot;
            return context;
        }

        VisibilityContext context{};
        context.reader_xid = getStatementXID();

        if (isolation_level_ != IsolationLevel::READ_COMMITTED_READ_CONSISTENCY)
        {
            context.valid = true;
            context.mode = VisibilityMode::READ_CURRENT_TRANSACTION;
            return context;
        }

        if (statement_io_active_)
        {
            if (statement_transaction_snapshot_ == nullptr)
            {
                context.reason = VisibilityReason::MISSING_SNAPSHOT;
                return context;
            }

            context.valid = true;
            context.mode = VisibilityMode::SNAPSHOT;
            context.snapshot = statement_transaction_snapshot_.get();
            return context;
        }

        context.valid = true;
        context.mode = VisibilityMode::READ_CURRENT_VERSION;
        return context;
    }

    Status ConnectionContext::ensureCurrentSchemaEpochInitialized(ErrorContext* ctx)
    {
        if (isForensicReplayActive())
        {
            current_schema_epoch_uuid_ = forensic_replay_binding_->schema_epoch_uuid;
            return Status::OK;
        }
        if (!db_ || isZeroUuidLocal(current_transaction_uuid_) || current_xid_ == 0)
        {
            return Status::OK;
        }
        if (!isZeroUuidLocal(current_schema_epoch_uuid_))
        {
            return Status::OK;
        }

        CatalogManager* catalog = db_->catalog_manager();
        if (!catalog)
        {
            return Status::OK;
        }

        CatalogManager::SchemaEpochCatalogInfo latest{};
        Status status = catalog->getLatestSchemaEpochCatalogEntry(db_->uuid(), latest, ctx);
        if (status == Status::OK)
        {
            current_schema_epoch_uuid_ = latest.schema_epoch_uuid;
            return Status::OK;
        }
        if (status != Status::NOT_FOUND)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to load latest schema epoch catalog entry");
            return status;
        }

        std::string manifest;
        status = catalog->buildCurrentSchemaEpochDefinitionManifest(manifest, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to build current schema epoch definition manifest");
            return status;
        }

        CatalogManager::SchemaEpochCatalogInfo epoch{};
        epoch.database_id = db_->uuid();
        epoch.origin_tx_uuid = current_transaction_uuid_;
        epoch.origin_txid = current_xid_;
        epoch.definition_manifest = manifest;
        epoch.created_time = nowMicros();
        status = catalog->appendSchemaEpochCatalogEntry(epoch, ctx);
        if (status == Status::OK)
        {
            current_schema_epoch_uuid_ = epoch.schema_epoch_uuid;
        }
        else
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to append schema epoch catalog entry");
        }
        return status;
    }

    Status ConnectionContext::parseForensicReplaySchemaManifest(
        const std::string& manifest,
        ForensicReplayBinding& binding_out,
        ErrorContext* ctx) const
    {
        binding_out.schema_definition_manifest = manifest;
        binding_out.tables_by_id.clear();
        binding_out.table_path_index.clear();
        if (manifest.empty())
        {
            return Status::OK;
        }

        json parsed;
        try
        {
            parsed = json::parse(manifest);
        }
        catch (const json::exception&)
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::PAGE_CORRUPT,
                              "FORENSIC_SCHEMA_HISTORY_PRUNED: schema epoch manifest is invalid");
            return Status::PAGE_CORRUPT;
        }

        const auto schemas_it = parsed.find("schemas");
        if (schemas_it == parsed.end() || !schemas_it->is_array())
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::PAGE_CORRUPT,
                              "FORENSIC_SCHEMA_HISTORY_PRUNED: schema epoch manifest is incomplete");
            return Status::PAGE_CORRUPT;
        }

        for (const auto& schema_doc : *schemas_it)
        {
            if (!schema_doc.is_object())
            {
                continue;
            }
            const std::string schema_name = schema_doc.value("schema_name", std::string());
            ID schema_id{};
            const std::string schema_uuid = schema_doc.value("schema_uuid", std::string());
            if (!schema_uuid.empty() && !parseUuidTextLocal(schema_uuid, schema_id))
            {
                continue;
            }

            const auto tables_it = schema_doc.find("tables");
            if (tables_it == schema_doc.end() || !tables_it->is_array())
            {
                continue;
            }

            for (const auto& table_doc : *tables_it)
            {
                if (!table_doc.is_object())
                {
                    continue;
                }

                ID table_id{};
                if (!parseUuidTextLocal(table_doc.value("table_uuid", std::string()), table_id))
                {
                    continue;
                }

                ForensicReplayBinding::HistoricalTableBinding binding{};
                auto& table = binding.table_info;
                table.table_id = table_id;
                table.schema_id = schema_id;
                table.table_name = table_doc.value("table_name", std::string());
                table.name_is_delimited = table_doc.value("table_name_is_delimited", false);
                (void)parseUuidTextLocal(table_doc.value("owner_uuid", std::string()), table.owner_id);
                table.root_gpid = table_doc.value("root_gpid", static_cast<GPID>(0));
                table.column_count = table_doc.value("column_count", 0u);
                table.row_count = table_doc.value("row_count", static_cast<uint64_t>(0));
                table.table_type = static_cast<uint8_t>(table_doc.value("table_type", 0));
                table.has_toast = table_doc.value("has_toast", false);
                (void)parseUuidTextLocal(table_doc.value("toast_table_uuid", std::string()), table.toast_table_id);
                (void)parseUuidTextLocal(table_doc.value("tablespace_uuid", std::string()), table.tablespace_uuid);
                table.tablespace_id = table_doc.value("tablespace_id", static_cast<uint16_t>(0));
                (void)parseUuidTextLocal(table_doc.value("default_charset_uuid", std::string()),
                                         table.default_charset_uuid);
                table.default_charset = table_doc.value("default_charset", static_cast<uint16_t>(0));
                table.default_collation_id = table_doc.value("default_collation_id", 0u);
                table.created_time = table_doc.value("created_time", static_cast<uint64_t>(0));
                table.last_modified_time =
                    table_doc.value("last_modified_time", static_cast<uint64_t>(0));
                table.policy_epoch = table_doc.value("policy_epoch", static_cast<uint64_t>(0));
                table.rls_enabled = table_doc.value("rls_enabled", false);
                table.rls_forced = table_doc.value("rls_forced", false);

                const auto columns_it = table_doc.find("columns");
                if (columns_it != table_doc.end() && columns_it->is_array())
                {
                    for (const auto& column_doc : *columns_it)
                    {
                        if (!column_doc.is_object())
                        {
                            continue;
                        }
                        HistoricalColumnInfo column{};
                        column.table_id = table.table_id;
                        (void)parseUuidTextLocal(column_doc.value("column_uuid", std::string()),
                                                 column.column_id);
                        column.column_name = column_doc.value("column_name", std::string());
                        column.name_is_delimited =
                            column_doc.value("column_name_is_delimited", false);
                        column.ordinal = column_doc.value("ordinal", static_cast<uint16_t>(0));
                        column.data_type = column_doc.value("data_type", static_cast<uint16_t>(0));
                        column.type_precision = column_doc.value("type_precision", 0u);
                        column.type_scale = column_doc.value("type_scale", 0u);
                        column.max_length = column_doc.value("max_length", 0u);
                        column.nullable = column_doc.value("nullable", true);
                        column.has_default = column_doc.value("has_default", false);
                        column.is_primary_key = column_doc.value("is_primary_key", false);
                        column.is_unique = column_doc.value("is_unique", false);
                        column.is_foreign_key = column_doc.value("is_foreign_key", false);
                        column.is_generated = column_doc.value("is_generated", false);
                        column.generated_type =
                            static_cast<uint8_t>(column_doc.value("generated_type", 0));
                        column.generation_expression =
                            column_doc.value("generation_expression", std::string());
                        column.is_identity = column_doc.value("is_identity", false);
                        column.identity_always = column_doc.value("identity_always", true);
                        (void)parseUuidTextLocal(column_doc.value("identity_sequence_uuid", std::string()),
                                                 column.identity_sequence_id);
                        column.storage_type = column_doc.value("storage_type", static_cast<uint8_t>(0));
                        column.with_timezone = column_doc.value("with_timezone", false);
                        (void)parseUuidTextLocal(column_doc.value("charset_uuid", std::string()),
                                                 column.charset_uuid);
                        column.charset = column_doc.value("charset", static_cast<uint16_t>(0));
                        (void)parseUuidTextLocal(column_doc.value("domain_uuid", std::string()),
                                                 column.domain_id);
                        column.is_array = column_doc.value("is_array", false);
                        column.array_size = column_doc.value("array_size", 0u);
                        (void)parseUuidTextLocal(column_doc.value("timezone_uuid", std::string()),
                                                 column.timezone_uuid);
                        column.timezone_hint =
                            column_doc.value("timezone_hint", static_cast<uint16_t>(0));
                        column.collation_id = column_doc.value("collation_id", 0u);
                        column.default_value = column_doc.value("default_value", std::string());
                        column.default_expr = column_doc.value("default_expr", std::string());
                        column.check_expr = column_doc.value("check_expr", std::string());
                        column.created_time =
                            column_doc.value("created_time", static_cast<uint64_t>(0));
                        binding.columns.push_back(std::move(column));
                    }
                }

                binding.qualified_name_upper =
                    IdentifierUtils::toUpper(schema_name + "." + table.table_name);
                binding.unqualified_name_upper = IdentifierUtils::toUpper(table.table_name);
                binding_out.table_path_index[binding.qualified_name_upper] = table.table_id;
                binding_out.tables_by_id[table.table_id] = std::move(binding);
            }
        }

        return Status::OK;
    }

    Status ConnectionContext::resolveForensicReplayTablePath(const std::string& qualified_name,
                                                             ID& table_id_out,
                                                             ErrorContext* ctx,
                                                             bool allow_search_path) const
    {
        table_id_out = ID{};
        if (!forensic_replay_binding_)
        {
            return Status::NOT_FOUND;
        }

        const std::string query_upper = IdentifierUtils::toUpper(qualified_name);
        auto direct = forensic_replay_binding_->table_path_index.find(query_upper);
        if (direct != forensic_replay_binding_->table_path_index.end())
        {
            table_id_out = direct->second;
            return Status::OK;
        }

        if (qualified_name.find('.') == std::string::npos && allow_search_path)
        {
            for (const auto& schema_name : search_path_)
            {
                if (schema_name.empty())
                {
                    continue;
                }
                const std::string candidate =
                    IdentifierUtils::toUpper(schema_name + "." + qualified_name);
                auto it = forensic_replay_binding_->table_path_index.find(candidate);
                if (it != forensic_replay_binding_->table_path_index.end())
                {
                    table_id_out = it->second;
                    return Status::OK;
                }
            }
        }

        SET_ERROR_CONTEXT(ctx,
                          Status::NOT_FOUND,
                          "FORENSIC_SCHEMA_HISTORY_PRUNED: historical table definition is unavailable");
        return Status::NOT_FOUND;
    }

    Status ConnectionContext::getForensicReplayTable(const ID& table_id,
                                                     HistoricalTableInfo& table_out,
                                                     ErrorContext* ctx) const
    {
        if (!forensic_replay_binding_)
        {
            return Status::NOT_FOUND;
        }
        auto it = forensic_replay_binding_->tables_by_id.find(table_id);
        if (it == forensic_replay_binding_->tables_by_id.end())
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::NOT_FOUND,
                              "FORENSIC_SCHEMA_HISTORY_PRUNED: historical table definition is unavailable");
            return Status::NOT_FOUND;
        }
        table_out = it->second.table_info;
        return Status::OK;
    }

    Status ConnectionContext::getForensicReplayColumns(
        const ID& table_id,
        std::vector<HistoricalColumnInfo>& columns_out,
        ErrorContext* ctx) const
    {
        if (!forensic_replay_binding_)
        {
            return Status::NOT_FOUND;
        }
        auto it = forensic_replay_binding_->tables_by_id.find(table_id);
        if (it == forensic_replay_binding_->tables_by_id.end())
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::NOT_FOUND,
                              "FORENSIC_SCHEMA_HISTORY_PRUNED: historical column definitions are unavailable");
            return Status::NOT_FOUND;
        }
        columns_out = it->second.columns;
        return Status::OK;
    }

    Status ConnectionContext::getForensicReplayColumn(const ID& table_id,
                                                      const std::string& column_name,
                                                      HistoricalColumnInfo& column_out,
                                                      ErrorContext* ctx) const
    {
        std::vector<HistoricalColumnInfo> columns;
        Status status = getForensicReplayColumns(table_id, columns, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        for (const auto& column : columns)
        {
            if (IdentifierUtils::namesMatch(column_name,
                                            false,
                                            column.column_name,
                                            column.name_is_delimited))
            {
                column_out = column;
                return Status::OK;
            }
        }

        SET_ERROR_CONTEXT(ctx,
                          Status::NOT_FOUND,
                          "FORENSIC_SCHEMA_HISTORY_PRUNED: historical column definition is unavailable");
        return Status::NOT_FOUND;
    }

    Status ConnectionContext::listForensicReplayTables(
        const ID& schema_id,
        std::vector<HistoricalTableInfo>& tables_out,
        ErrorContext* ctx) const
    {
        tables_out.clear();
        if (!forensic_replay_binding_)
        {
            return Status::NOT_FOUND;
        }
        for (const auto& entry : forensic_replay_binding_->tables_by_id)
        {
            if (isZeroUuidLocal(schema_id) || entry.second.table_info.schema_id == schema_id)
            {
                tables_out.push_back(entry.second.table_info);
            }
        }
        if (tables_out.empty() && !isZeroUuidLocal(schema_id))
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::NOT_FOUND,
                              "FORENSIC_SCHEMA_HISTORY_PRUNED: historical schema has no retained tables");
            return Status::NOT_FOUND;
        }
        std::sort(tables_out.begin(), tables_out.end(), [](const HistoricalTableInfo& lhs,
                                                           const HistoricalTableInfo& rhs) {
            if (lhs.table_name != rhs.table_name)
            {
                return lhs.table_name < rhs.table_name;
            }
            return lhs.table_id.bytes < rhs.table_id.bytes;
        });
        return Status::OK;
    }

    Status ConnectionContext::initialize(ErrorContext *ctx)
    {
        if (db_ != nullptr && db_->startup_quarantine_active())
        {
            default_is_read_only_ = true;
            next_is_read_only_ = true;
            is_read_only_ = true;
        }

        // Start initial transaction
        Status s = beginNewTransaction(ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION,
                      "Failed to initialize connection context: %d (%s)",
                      static_cast<int>(s),
                      (ctx != nullptr && !ctx->message.empty()) ? ctx->message.c_str()
                                                                 : "no detail");
            return s;
        }

        if (db_)
        {
            db_->registerConnectionContext(this);
        }

        LOG_DEBUG(TRANSACTION, "Initialized connection context: proc_id=%u, xid=%lu", proc_id_,
                  current_xid_);

        return Status::OK;
    }

    Status ConnectionContext::openForensicReplayByTransactionUuid(const ID& tx_uuid,
                                                                  ErrorContext* ctx)
    {
        if (!db_ || isZeroUuidLocal(tx_uuid))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "FORENSIC_SELECTOR_INVALID: tx_uuid is required");
            return Status::INVALID_ARGUMENT;
        }
        if (isForensicReplayActive())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_TRANSACTION_STATE,
                              "forensic replay session is already bound");
            return Status::INVALID_TRANSACTION_STATE;
        }

        CatalogManager* catalog = db_->catalog_manager();
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        std::vector<CatalogManager::RuntimeTransactionCatalogInfo> rows;
        Status status = catalog->listRuntimeTransactionCatalogEntries(db_->uuid(), rows, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        const auto score_row = [](const CatalogManager::RuntimeTransactionCatalogInfo& row) {
            int score = 0;
            if (row.state != CatalogManager::RuntimeTransactionState::IN_PROGRESS)
            {
                score += 4;
            }
            if (!isZeroUuidLocal(row.forensic_snapshot_capsule_uuid))
            {
                score += 2;
            }
            if (row.has_end_time)
            {
                score += 1;
            }
            return score;
        };

        const CatalogManager::RuntimeTransactionCatalogInfo* selected = nullptr;
        for (const auto& row : rows)
        {
            if (row.tx_uuid != tx_uuid)
            {
                continue;
            }
            if (selected == nullptr)
            {
                selected = &row;
                continue;
            }

            const int row_score = score_row(row);
            const int selected_score = score_row(*selected);
            if (row_score > selected_score ||
                (row_score == selected_score &&
                 (row.last_modified_time > selected->last_modified_time ||
                  (row.last_modified_time == selected->last_modified_time &&
                   row.txid > selected->txid))))
            {
                selected = &row;
            }
        }

        if (selected == nullptr)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              "FORENSIC_SELECTOR_INVALID: tx_uuid was not found");
            return Status::NOT_FOUND;
        }
        if (isZeroUuidLocal(selected->forensic_snapshot_capsule_uuid))
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              "FORENSIC_CAPSULE_UNAVAILABLE: retained snapshot capsule missing");
            return Status::NOT_FOUND;
        }
        return openForensicReplayByTxid(selected->txid, ctx);
    }

    Status ConnectionContext::openForensicReplayByTxid(uint64_t txid, ErrorContext* ctx)
    {
        if (!db_ || txid == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "FORENSIC_SELECTOR_INVALID: txid is required");
            return Status::INVALID_ARGUMENT;
        }
        if (isForensicReplayActive())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_TRANSACTION_STATE,
                              "forensic replay session is already bound");
            return Status::INVALID_TRANSACTION_STATE;
        }

        CatalogManager* catalog = db_->catalog_manager();
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        CatalogManager::RuntimeTransactionCatalogInfo row{};
        Status status = catalog->getRuntimeTransactionCatalogEntry(txid, row, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              "FORENSIC_SELECTOR_INVALID: txid was not found");
            return Status::NOT_FOUND;
        }
        if (isZeroUuidLocal(row.forensic_snapshot_capsule_uuid))
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              "FORENSIC_CAPSULE_UNAVAILABLE: retained snapshot capsule missing");
            return Status::NOT_FOUND;
        }
        return openForensicReplayByCapsuleUuid(row.forensic_snapshot_capsule_uuid, ctx);
    }

    Status ConnectionContext::openForensicReplayByCapsuleUuid(const ID& capsule_uuid,
                                                              ErrorContext* ctx)
    {
        if (!db_ || isZeroUuidLocal(capsule_uuid))
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "FORENSIC_SELECTOR_INVALID: capsule_uuid is required");
            return Status::INVALID_ARGUMENT;
        }
        if (isForensicReplayActive())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_TRANSACTION_STATE,
                              "forensic replay session is already bound");
            return Status::INVALID_TRANSACTION_STATE;
        }

        CatalogManager* catalog = db_->catalog_manager();
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "catalog manager not available");
            return Status::INVALID_ARGUMENT;
        }

        CatalogManager::ForensicSnapshotCapsuleCatalogInfo capsule{};
        Status status = catalog->getForensicSnapshotCapsuleCatalogEntry(capsule_uuid, capsule, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND,
                              "FORENSIC_CAPSULE_UNAVAILABLE: capsule was not found");
            return Status::NOT_FOUND;
        }
        if (capsule.database_id != db_->uuid())
        {
            SET_ERROR_CONTEXT(ctx, Status::PERMISSION_DENIED,
                              "FORENSIC_REPLAY_NOT_ALLOWED: capsule belongs to another database");
            return Status::PERMISSION_DENIED;
        }

        auto replay_binding = std::make_unique<ForensicReplayBinding>();
        replay_binding->capsule_uuid = capsule.capsule_id;
        replay_binding->tx_uuid = capsule.tx_uuid;
        replay_binding->txid = capsule.txid;
        replay_binding->schema_epoch_uuid = capsule.schema_epoch_uuid;
        replay_binding->snapshot_kind = capsule.snapshot_kind;
        status = parseForensicReplaySnapshot(capsule.active_tx_manifest,
                                             capsule.visibility_manifest,
                                             replay_binding->snapshot,
                                             ctx);
        if (status != Status::OK)
        {
            return status;
        }
        if (!isZeroUuidLocal(capsule.schema_epoch_uuid))
        {
            CatalogManager::SchemaEpochCatalogInfo schema_epoch{};
            status = catalog->getSchemaEpochCatalogEntry(capsule.schema_epoch_uuid, schema_epoch, ctx);
            if (status != Status::OK)
            {
                SET_ERROR_CONTEXT(ctx,
                                  Status::NOT_FOUND,
                                  "FORENSIC_SCHEMA_HISTORY_PRUNED: schema epoch could not be resolved");
                return Status::NOT_FOUND;
            }
            status = parseForensicReplaySchemaManifest(
                schema_epoch.definition_manifest, *replay_binding, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        stageTransactionSettings(IsolationLevel::SNAPSHOT,
                                 true,
                                 wait_for_locks_,
                                 lock_timeout_seconds_,
                                 ReadCommittedMode::READ_CONSISTENCY);

        if (current_xid_ != 0)
        {
            status = endCurrentTransaction(false, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        applyStagedSettings();
        forensic_replay_binding_ = std::move(replay_binding);
        current_schema_epoch_uuid_ = forensic_replay_binding_->schema_epoch_uuid;
        status = beginNewTransaction(ctx);
        if (status != Status::OK)
        {
            forensic_replay_binding_.reset();
            std::memset(&current_schema_epoch_uuid_, 0, sizeof(current_schema_epoch_uuid_));
            return status;
        }

        return Status::OK;
    }

    Status ConnectionContext::closeForensicReplay(ErrorContext* ctx)
    {
        if (!isForensicReplayActive())
        {
            return Status::OK;
        }

        stageTransactionSettings(default_isolation_level_,
                                 default_is_read_only_,
                                 default_wait_for_locks_,
                                 default_lock_timeout_seconds_,
                                 default_read_committed_mode_);

        Status status = Status::OK;
        if (current_xid_ != 0)
        {
            status = endCurrentTransaction(false, ctx);
            if (status != Status::OK)
            {
                return status;
            }
        }

        forensic_replay_binding_.reset();
        std::memset(&current_schema_epoch_uuid_, 0, sizeof(current_schema_epoch_uuid_));
        applyStagedSettings();
        return beginNewTransaction(ctx);
    }

    void ConnectionContext::stageTransactionalDdlBatch(uint8_t object_type,
                                                       const ID& object_id,
                                                       const std::string& operation_class,
                                                       uint64_t statement_hash,
                                                       const std::string& statement_text)
    {
        if (current_xid_ == 0 || isForensicReplayActive() || isZeroUuidLocal(object_id))
        {
            return;
        }

        PendingTransactionalDdlBatch batch{};
        batch.object_type = object_type;
        batch.object_id = object_id;
        batch.operation_class = operation_class.empty() ? "DDL" : operation_class;
        batch.statement_hash = statement_hash;
        batch.statement_text = statement_text;

        if (!pending_transactional_ddl_batches_.empty())
        {
            const auto& prior = pending_transactional_ddl_batches_.back();
            if (prior.object_type == batch.object_type &&
                prior.object_id == batch.object_id &&
                prior.operation_class == batch.operation_class &&
                prior.statement_hash == batch.statement_hash &&
                prior.statement_text == batch.statement_text)
            {
                return;
            }
        }

        pending_transactional_ddl_batches_.push_back(std::move(batch));
    }

    Status ConnectionContext::flushCommittedTransactionalDdl(uint64_t commit_seqno,
                                                             ErrorContext* ctx)
    {
        if (!db_ ||
            current_xid_ == 0 ||
            isZeroUuidLocal(current_transaction_uuid_) ||
            pending_transactional_ddl_batches_.empty())
        {
            return Status::OK;
        }

        CatalogManager* catalog = db_->catalog_manager();
        if (!catalog)
        {
            return Status::OK;
        }

        const ID schema_epoch_before_uuid = current_schema_epoch_uuid_;
        std::string definition_manifest;
        Status status = catalog->buildCurrentSchemaEpochDefinitionManifest(definition_manifest, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        CatalogManager::SchemaEpochCatalogInfo schema_epoch{};
        schema_epoch.database_id = db_->uuid();
        schema_epoch.has_commit_seqno = commit_seqno != 0;
        schema_epoch.commit_seqno = commit_seqno;
        schema_epoch.origin_tx_uuid = current_transaction_uuid_;
        schema_epoch.origin_txid = current_xid_;
        schema_epoch.has_parent_schema_epoch_uuid = !isZeroUuidLocal(schema_epoch_before_uuid);
        schema_epoch.parent_schema_epoch_uuid = schema_epoch_before_uuid;
        schema_epoch.definition_manifest = definition_manifest;
        status = catalog->appendSchemaEpochCatalogEntry(schema_epoch, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        json payload = json::object();
        payload["database_uuid"] = db_->uuid().toString();
        payload["tx_uuid"] = current_transaction_uuid_.toString();
        payload["txid"] = current_xid_;
        payload["schema_epoch_before_uuid"] = schema_epoch_before_uuid.toString();
        payload["schema_epoch_after_uuid"] = schema_epoch.schema_epoch_uuid.toString();
        payload["operation_count"] = pending_transactional_ddl_batches_.size();
        payload["operations"] = json::array();

        for (const auto& op : pending_transactional_ddl_batches_)
        {
            json op_doc = json::object();
            op_doc["object_uuid"] = op.object_id.toString();
            op_doc["object_type"] = objectTypeLabel(op.object_type);
            op_doc["operation_class"] = op.operation_class;
            op_doc["statement_hash"] = op.statement_hash;
            op_doc["statement_text"] = op.statement_text;
            payload["operations"].push_back(std::move(op_doc));
        }

        CatalogManager::TransactionLineageEventCatalogInfo ddl_event{};
        ddl_event.tx_uuid = current_transaction_uuid_;
        ddl_event.txid = current_xid_;
        ddl_event.event_kind = CatalogManager::TransactionLineageEventKind::TX_DDL_BATCH;
        ddl_event.session_id = effectiveSessionId();
        ddl_event.connection_id = attachment_id_;
        ddl_event.user_id = current_user_id_;
        ddl_event.role_id = active_role_id_;
        if (pending_transactional_ddl_batches_.size() == 1)
        {
            ddl_event.object_id = pending_transactional_ddl_batches_.front().object_id;
        }
        if (pending_transactional_ddl_batches_.back().statement_hash != 0)
        {
            ddl_event.has_statement_hash = true;
            ddl_event.statement_hash = pending_transactional_ddl_batches_.back().statement_hash;
        }
        ddl_event.payload_json = payload.dump();
        status = catalog->appendTransactionLineageEventCatalogEntry(ddl_event, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        current_schema_epoch_uuid_ = schema_epoch.schema_epoch_uuid;
        pending_transactional_ddl_batches_.clear();
        return Status::OK;
    }

    Status ConnectionContext::persistRuntimeTransactionState(
        uint8_t state_code,
        uint64_t txid,
        const ID& tx_uuid,
        const ID& capsule_uuid,
        const ID& schema_epoch_uuid,
        bool has_commit_seqno,
        uint64_t commit_seqno,
        uint64_t end_time,
        ErrorContext* ctx)
    {
        if (!db_ || txid == 0)
        {
            return Status::OK;
        }
        if (!shouldPersistTransactionEvidence())
        {
            return Status::OK;
        }

        CatalogManager* catalog = db_->catalog_manager();
        if (!catalog)
        {
            return Status::OK;
        }

        CatalogManager::RuntimeTransactionCatalogInfo info{};
        info.txid = txid;
        info.tx_uuid = tx_uuid;
        info.database_id = db_->uuid();
        info.session_id = session_id_;
        if (!isZeroUuidLocal(info.session_id))
        {
            CatalogManager::SessionInfo session_info{};
            ErrorContext session_ctx;
            if (catalog->getSession(info.session_id, session_info, &session_ctx) != Status::OK)
            {
                // Runtime transaction retention must not fail user-visible commit/rollback
                // just because the session row was already rotated or closed.
                info.session_id = ID{};
            }
        }
        info.connection_id = ID{};
        info.user_id = current_user_id_;
        info.role_id = active_role_id_;
        info.emulation_engine = catalogEmulationEngineForMode(emulation_mode_);
        info.isolation_level = static_cast<uint8_t>(isolation_level_);
        info.read_only = is_read_only_;
        info.autocommit = autocommit_mode_ && !autocommit_suspended_;
        info.state = static_cast<CatalogManager::RuntimeTransactionState>(state_code);
        info.start_time = static_cast<uint64_t>(xact_start_time_.count());
        info.has_end_time = info.state != CatalogManager::RuntimeTransactionState::IN_PROGRESS;
        info.end_time = info.has_end_time ? end_time : 0;
        info.has_commit_seqno = has_commit_seqno;
        info.commit_seqno = has_commit_seqno ? commit_seqno : 0;
        info.schema_epoch_uuid = schema_epoch_uuid;
        info.forensic_snapshot_capsule_uuid = capsule_uuid;
        info.has_last_statement_hash = last_statement_hash_ != 0;
        info.last_statement_hash = last_statement_hash_;
        info.has_last_statement_time = last_statement_time_ != 0;
        info.last_statement_time = last_statement_time_;
        info.has_last_error_code = last_error_code_ != 0;
        info.last_error_code = last_error_code_;
        info.last_sqlstate = last_sqlstate_;
        Status status = catalog->upsertRuntimeTransactionCatalogEntry(info, ctx);
        if (status != Status::OK)
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to upsert runtime transaction catalog entry");
        }
        return status;
    }

    Status ConnectionContext::appendTransactionLineageBegin(ErrorContext* ctx)
    {
        if (!db_ || current_xid_ == 0 || isZeroUuidLocal(current_transaction_uuid_))
        {
            return Status::OK;
        }
        if (!shouldPersistTransactionEvidence())
        {
            return Status::OK;
        }

        CatalogManager* catalog = db_->catalog_manager();
        if (!catalog)
        {
            return Status::OK;
        }

        CatalogManager::TransactionLineageEventCatalogInfo begin{};
        begin.tx_uuid = current_transaction_uuid_;
        begin.txid = current_xid_;
        begin.event_kind = CatalogManager::TransactionLineageEventKind::TX_BEGIN;
        begin.session_id = effectiveSessionId();
        begin.connection_id = attachment_id_;
        begin.user_id = current_user_id_;
        begin.role_id = active_role_id_;
        begin.payload_json = buildTransactionBeginPayload(db_, this);
        Status status = catalog->appendTransactionLineageEventCatalogEntry(begin, ctx);
        if (status == Status::OK)
        {
            lineage_root_event_id_ = begin.lineage_event_id;
        }
        else
        {
            SET_ERROR_CONTEXT(ctx, status, "Failed to append TX_BEGIN lineage event");
        }
        return status;
    }

    Status ConnectionContext::appendTransactionLineageTerminal(bool committed,
                                                               uint64_t txid,
                                                               const ID& tx_uuid,
                                                               uint64_t start_time,
                                                               uint64_t end_time,
                                                               ErrorContext* ctx)
    {
        if (!db_ || txid == 0 || isZeroUuidLocal(tx_uuid))
        {
            return Status::OK;
        }
        if (!shouldPersistTransactionEvidence())
        {
            return Status::OK;
        }

        CatalogManager* catalog = db_->catalog_manager();
        if (!catalog)
        {
            return Status::OK;
        }

        CatalogManager::TransactionLineageEventCatalogInfo context{};
        context.tx_uuid = tx_uuid;
        context.txid = txid;
        context.event_kind = CatalogManager::TransactionLineageEventKind::TX_CONTEXT_BOUND;
        context.session_id = effectiveSessionId();
        context.connection_id = attachment_id_;
        context.user_id = current_user_id_;
        context.role_id = active_role_id_;
        context.payload_json = buildTransactionContextPayload(this);
        Status status = catalog->appendTransactionLineageEventCatalogEntry(context, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        CatalogManager::TransactionLineageEventCatalogInfo terminal{};
        terminal.tx_uuid = tx_uuid;
        terminal.txid = txid;
        terminal.event_kind = committed
            ? CatalogManager::TransactionLineageEventKind::TX_COMMIT
            : CatalogManager::TransactionLineageEventKind::TX_ROLLBACK;
        terminal.session_id = effectiveSessionId();
        terminal.connection_id = attachment_id_;
        terminal.user_id = current_user_id_;
        terminal.role_id = active_role_id_;
        terminal.has_statement_hash = last_statement_hash_ != 0;
        terminal.statement_hash = last_statement_hash_;
        terminal.payload_json =
            buildTransactionTerminalPayload(db_, this, committed, start_time, end_time);
        return catalog->appendTransactionLineageEventCatalogEntry(terminal, ctx);
    }

    Status ConnectionContext::createForensicSnapshotCapsule(bool committed,
                                                            uint64_t txid,
                                                            const ID& tx_uuid,
                                                            uint64_t commit_seqno,
                                                            uint64_t end_time,
                                                            ID& capsule_uuid_out,
                                                            ErrorContext* ctx)
    {
        capsule_uuid_out = ID{};
        if (!db_ ||
            txid == 0 ||
            isZeroUuidLocal(tx_uuid) ||
            !retained_transaction_snapshot_ ||
            isZeroUuidLocal(lineage_root_event_id_) ||
            isForensicReplayActive())
        {
            return Status::OK;
        }
        if (!shouldPersistTransactionEvidence())
        {
            return Status::OK;
        }

        CatalogManager* catalog = db_->catalog_manager();
        if (!catalog)
        {
            return Status::OK;
        }

        CatalogManager::ForensicSnapshotCapsuleCatalogInfo capsule{};
        capsule.database_id = db_->uuid();
        capsule.tx_uuid = tx_uuid;
        capsule.txid = txid;
        capsule.has_commit_seqno = committed && commit_seqno != 0;
        capsule.commit_seqno = committed ? commit_seqno : 0;
        capsule.snapshot_kind = "TRANSACTION_START";
        capsule.schema_epoch_uuid = current_schema_epoch_uuid_;
        capsule.active_tx_manifest = buildActiveTxManifest(*retained_transaction_snapshot_);
        capsule.visibility_manifest =
            buildVisibilityManifest(*retained_transaction_snapshot_, isolation_level_);
        capsule.lineage_root_event_id = lineage_root_event_id_;
        capsule.created_time = (end_time == 0) ? nowMicros() : end_time;
        capsule.retention_deadline_time =
            (std::numeric_limits<uint64_t>::max() - capsule.created_time <
             kForensicReplayRetentionMicros)
                ? std::numeric_limits<uint64_t>::max()
                : capsule.created_time + kForensicReplayRetentionMicros;
        capsule.status = committed ? "COMMITTED" : "ABORTED";
        capsule.is_valid = true;

        Status status = catalog->appendForensicSnapshotCapsuleCatalogEntry(capsule, ctx);
        if (status == Status::OK)
        {
            capsule_uuid_out = capsule.capsule_id;
        }
        return status;
    }

    bool ConnectionContext::shouldPersistTransactionEvidence() const
    {
        if (isForensicReplayActive())
        {
            return true;
        }
        if (!pending_transactional_ddl_batches_.empty())
        {
            return true;
        }
        return !isZeroUuidLocal(current_user_id_) ||
               !isZeroUuidLocal(session_user_id_) ||
               !isZeroUuidLocal(active_role_id_) ||
               !isZeroUuidLocal(session_id_) ||
               !isZeroUuidLocal(protocol_session_id_);
    }

    void ConnectionContext::refreshActiveTransactionAttribution()
    {
        if (current_xid_ == 0)
        {
            return;
        }
        if (!shouldPersistTransactionEvidence())
        {
            return;
        }

        ErrorContext ctx;
        Status status = persistRuntimeTransactionState(
            static_cast<uint8_t>(CatalogManager::RuntimeTransactionState::IN_PROGRESS),
            current_xid_,
            current_transaction_uuid_,
            forensic_replay_binding_ ? forensic_replay_binding_->capsule_uuid : ID{},
            current_schema_epoch_uuid_,
            false,
            0,
            0,
            &ctx);
        if (status != Status::OK)
        {
            LOG_WARNING(TRANSACTION,
                        "Failed to refresh runtime transaction attribution: proc_id=%u, xid=%lu, status=%d",
                        proc_id_, current_xid_, static_cast<int>(status));
        }

        if (isZeroUuidLocal(lineage_root_event_id_))
        {
            ErrorContext lineage_ctx;
            Status lineage_status = appendTransactionLineageBegin(&lineage_ctx);
            if (lineage_status != Status::OK)
            {
                LOG_WARNING(TRANSACTION,
                            "Failed to initialize retained transaction lineage after attribution refresh: proc_id=%u, xid=%lu, status=%d, detail=%s",
                            proc_id_,
                            current_xid_,
                            static_cast<int>(lineage_status),
                            lineage_ctx.message.c_str());
            }
        }
    }

    Status ConnectionContext::cleanupTempTablesOnCommit(ErrorContext *ctx)
    {
        if (!db_)
        {
            return Status::OK;
        }

        struct CurrentContextGuard
        {
            ConnectionContext* previous = nullptr;
            bool changed = false;

            explicit CurrentContextGuard(ConnectionContext* current)
                : previous(ConnectionContext::getCurrent())
            {
                if (current && current != previous)
                {
                    ConnectionContext::setCurrent(current);
                    changed = true;
                }
            }

            ~CurrentContextGuard()
            {
                if (changed)
                {
                    ConnectionContext::setCurrent(previous);
                }
            }
        };

        CurrentContextGuard ctx_guard(this);

        CatalogManager *catalog = db_->catalog_manager();
        StorageEngine *storage = db_->storage_engine();
        if (!catalog || !storage)
        {
            return Status::OK;
        }

        ID session_id = effectiveSessionId();
        std::vector<CatalogManager::TableInfo> tables;
        Status status = catalog->listTemporaryTablesForSession(session_id, tables, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        for (const auto& table : tables)
        {
            if (table.temp_on_commit == CatalogManager::TempOnCommitAction::DELETE_ROWS)
            {
                status = storage->deleteTuplesForSession(table.table_id, session_id, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }
            else if (table.temp_on_commit == CatalogManager::TempOnCommitAction::DROP &&
                     table.temp_metadata_scope == CatalogManager::TempMetadataScope::SESSION)
            {
                status = catalog->dropTable(table.table_id, true, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }
        }

        return Status::OK;
    }

    Status ConnectionContext::cleanupTempTablesOnSessionEnd(ErrorContext *ctx)
    {
        if (!db_)
        {
            return Status::OK;
        }

        struct CurrentContextGuard
        {
            ConnectionContext* previous = nullptr;
            bool changed = false;

            explicit CurrentContextGuard(ConnectionContext* current)
                : previous(ConnectionContext::getCurrent())
            {
                if (current && current != previous)
                {
                    ConnectionContext::setCurrent(current);
                    changed = true;
                }
            }

            ~CurrentContextGuard()
            {
                if (changed)
                {
                    ConnectionContext::setCurrent(previous);
                }
            }
        };

        CurrentContextGuard ctx_guard(this);

        CatalogManager *catalog = db_->catalog_manager();
        StorageEngine *storage = db_->storage_engine();
        if (!catalog || !storage)
        {
            return Status::OK;
        }

        ID session_id = effectiveSessionId();
        std::vector<CatalogManager::ViewInfo> views;
        Status status = catalog->listTemporaryViewsForSession(session_id, views, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        for (const auto& view : views)
        {
            status = catalog->dropView(view.view_id, true, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND)
            {
                return status;
            }
        }

        std::vector<CatalogManager::TableInfo> tables;
        status = catalog->listTemporaryTablesForSession(session_id, tables, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        for (const auto& table : tables)
        {
            if (table.temp_data_scope != CatalogManager::TempDataScope::NONE)
            {
                status = storage->deleteTuplesForSession(table.table_id, session_id, ctx);
                if (status != Status::OK)
                {
                    return status;
                }
            }

            if (table.temp_metadata_scope == CatalogManager::TempMetadataScope::SESSION)
            {
                status = catalog->dropTable(table.table_id, true, ctx);
                if (status != Status::OK && status != Status::NOT_FOUND)
                {
                    return status;
                }
            }
        }

        std::vector<CatalogManager::SequenceInfo> sequences;
        status = catalog->listTemporarySequencesForSession(session_id, sequences, ctx);
        if (status != Status::OK)
        {
            return status;
        }

        for (const auto& seq : sequences)
        {
            status = catalog->dropSequence(seq.sequence_id, true, ctx);
            if (status != Status::OK && status != Status::NOT_FOUND)
            {
                return status;
            }
        }

        return Status::OK;
    }

    Status ConnectionContext::commit(ErrorContext *ctx)
    {
        if (isForensicReplayActive())
        {
            return closeForensicReplay(ctx);
        }
        if (current_xid_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No active transaction to commit");
            return Status::INVALID_ARGUMENT;
        }

        LOG_INFO(TRANSACTION, "Commit start: proc_id=%u, xid=%lu", proc_id_, current_xid_);
        LOG_DEBUG(TRANSACTION, "Committing transaction: proc_id=%u, xid=%lu", proc_id_,
                  current_xid_);

        // 1. Check if termination has been requested (before commit)
        Status s = checkTerminationRequested(ctx);
        if (s != Status::OK)
        {
            // Termination requested - rollback instead of commit and return error
            LOG_WARNING(TRANSACTION,
                        "Termination requested, rolling back instead of committing: proc_id=%u, "
                        "xid=%lu",
                        proc_id_, current_xid_);
            rollback(nullptr); // Best effort rollback
            return s;
        }

        // 2. Apply ON COMMIT actions for temporary tables
        s = cleanupTempTablesOnCommit(ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION,
                      "Failed to apply ON COMMIT actions: proc_id=%u, xid=%lu, status=%d",
                      proc_id_, current_xid_, static_cast<int>(s));
            return s;
        }

        // 3. Commit current transaction
        s = endCurrentTransaction(true, ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION, "Failed to commit transaction: proc_id=%u, xid=%lu, status=%d",
                      proc_id_, current_xid_, static_cast<int>(s));
            return s;
        }

        // 4. Apply staged settings if any
        applyStagedSettings();

        // 5. ATOMICALLY start new transaction
        s = beginNewTransaction(ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION,
                      "Failed to start new transaction after commit: proc_id=%u, status=%d",
                      proc_id_, static_cast<int>(s));
            return s;
        }

        LOG_DEBUG(TRANSACTION, "Started new transaction after commit: proc_id=%u, new_xid=%lu",
                  proc_id_, current_xid_);
        LOG_INFO(TRANSACTION, "Commit complete: proc_id=%u, xid=%lu", proc_id_, current_xid_);

        return Status::OK;
    }

    Status ConnectionContext::rollback(ErrorContext *ctx)
    {
        if (isForensicReplayActive())
        {
            return closeForensicReplay(ctx);
        }
        if (current_xid_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No active transaction to rollback");
            return Status::INVALID_ARGUMENT;
        }

        LOG_INFO(TRANSACTION, "Rollback start: proc_id=%u, xid=%lu", proc_id_, current_xid_);
        LOG_DEBUG(TRANSACTION, "Rolling back transaction: proc_id=%u, xid=%lu", proc_id_,
                  current_xid_);

        // 1. Check if termination has been requested (before rollback)
        Status s = checkTerminationRequested(ctx);
        bool termination_requested = (s != Status::OK);

        // 2. Rollback current transaction (always succeeds)
        s = endCurrentTransaction(false, ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Rollback encountered error: proc_id=%u, xid=%lu, status=%d",
                        proc_id_, current_xid_, static_cast<int>(s));
            // Continue anyway - rollback should be best-effort
        }

        // If termination was requested, don't start a new transaction - just return error
        if (termination_requested)
        {
            LOG_WARNING(TRANSACTION,
                        "Termination requested, not starting new transaction after rollback: "
                        "proc_id=%u",
                        proc_id_);
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                              "Connection terminated due to long-running transaction");
            return Status::IO_ERROR;
        }

        // 3. Apply staged settings if any
        applyStagedSettings();

        // 4. Start new transaction
        s = beginNewTransaction(ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION,
                      "Failed to start new transaction after rollback: proc_id=%u, status=%d",
                      proc_id_, static_cast<int>(s));
            return s;
        }

        LOG_DEBUG(TRANSACTION, "Started new transaction after rollback: proc_id=%u, new_xid=%lu",
                  proc_id_, current_xid_);
        LOG_INFO(TRANSACTION, "Rollback complete: proc_id=%u, xid=%lu", proc_id_, current_xid_);

        return Status::OK;
    }

    Status ConnectionContext::prepareTransaction(const std::string& gid, ErrorContext *ctx)
    {
        if (current_xid_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "No active transaction to prepare");
            return Status::INVALID_ARGUMENT;
        }

        if (gid.empty())
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "GID required for prepare");
            return Status::INVALID_ARGUMENT;
        }

        LOG_DEBUG(TRANSACTION, "Preparing transaction: proc_id=%u, xid=%lu, gid=%s",
                  proc_id_, current_xid_, gid.c_str());

        Status s = checkTerminationRequested(ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION,
                        "Termination requested, rolling back instead of preparing: proc_id=%u, "
                        "xid=%lu",
                        proc_id_, current_xid_);
            rollback(nullptr);
            return s;
        }

        const uint32_t prepared_owner_proc_id = proc_id_;
        uint32_t new_proc_id = UINT32_MAX;
        s = ProcArrayManager::registerBackend(&new_proc_id, ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION,
                      "Failed to allocate fresh backend slot before prepare: proc_id=%u, xid=%lu, status=%d",
                      proc_id_,
                      current_xid_,
                      static_cast<int>(s));
            return s;
        }

        if (!isZeroUuidLocal(session_id_))
        {
            Status session_status = ProcArrayManager::setSessionId(new_proc_id, session_id_, ctx);
            if (session_status != Status::OK)
            {
                ProcArrayManager::unregisterBackend(new_proc_id, nullptr);
                return session_status;
            }
        }

        s = txn_manager_->prepareTransaction(proc_id_, current_xid_, gid, current_user_id_, ctx);
        if (s != Status::OK)
        {
            ProcArrayManager::unregisterBackend(new_proc_id, nullptr);
            LOG_ERROR(TRANSACTION,
                      "Failed to prepare transaction: proc_id=%u, xid=%lu, status=%d",
                      proc_id_, current_xid_, static_cast<int>(s));
            return s;
        }

        Status detach_status = ProcArrayManager::detachPreparedOwner(prepared_owner_proc_id, ctx);
        if (detach_status != Status::OK)
        {
            LOG_WARNING(TRANSACTION,
                        "Failed to detach prepared lock owner from ProcArray: proc_id=%u, xid=%lu, status=%d",
                        prepared_owner_proc_id,
                        current_xid_,
                        static_cast<int>(detach_status));
        }

        statement_xid_ = 0;
        savepoint_stack_.clear();
        savepoint_level_ = 0;
        command_id_ = 0;
        current_xid_ = 0;
        transaction_start_oit_ = 0;
        transaction_start_oat_ = 0;
        transaction_start_ost_ = 0;
        xact_start_time_ = std::chrono::microseconds(0);
        proc_id_ = new_proc_id;
        if (db_)
        {
            db_->rebindConnectionContext(proc_id_, this);
        }

        applyStagedSettings();

        s = beginNewTransaction(ctx);
        if (s != Status::OK)
        {
            LOG_ERROR(TRANSACTION,
                      "Failed to start new transaction after prepare: proc_id=%u, status=%d",
                      proc_id_, static_cast<int>(s));
            return s;
        }

        LOG_DEBUG(TRANSACTION, "Started new transaction after prepare: proc_id=%u, new_xid=%lu",
                  proc_id_, current_xid_);

        return Status::OK;
    }

    Status ConnectionContext::shutdownTransaction(ErrorContext *ctx)
    {
        if (current_xid_ == 0)
        {
            return Status::OK;
        }

        // Disconnects should not start a new transaction; end the current one in-place.
        return endCurrentTransaction(false, ctx);
    }

    Status ConnectionContext::startTransaction(bool read_only, IsolationLevel isolation_level,
                                               bool commit_outstanding, ErrorContext *ctx)
    {
        return startTransaction(read_only, isolation_level, ReadCommittedMode::DEFAULT,
                                commit_outstanding, ctx);
    }

    Status ConnectionContext::startTransaction(bool read_only, IsolationLevel isolation_level,
                                               ReadCommittedMode read_committed_mode,
                                               bool commit_outstanding, ErrorContext *ctx)
    {
        if (isForensicReplayActive())
        {
            SET_ERROR_CONTEXT(ctx,
                              Status::READ_ONLY_TRANSACTION,
                              "FORENSIC_REPLAY_MUTATION_FORBIDDEN: close replay before changing transaction mode");
            return Status::READ_ONLY_TRANSACTION;
        }
        if (db_ != nullptr && db_->startup_quarantine_active() && !read_only)
        {
            SET_ERROR_CONTEXT(
                ctx,
                Status::READ_ONLY_TRANSACTION,
                "STARTUP_QUARANTINE_READ_ONLY: startup corruption policy requires read-only transactions");
            return Status::READ_ONLY_TRANSACTION;
        }
        ReadCommittedMode effective_mode = read_committed_mode;
        if (effective_mode == ReadCommittedMode::DEFAULT)
        {
            effective_mode = read_committed_mode_;
        }

        LOG_DEBUG(
            TRANSACTION,
            "START TRANSACTION: proc_id=%u, read_only=%d, isolation=%d, rc_mode=%d, commit_outstanding=%d",
            proc_id_, read_only, static_cast<int>(isolation_level),
            static_cast<int>(effective_mode), commit_outstanding);

        if (commit_outstanding)
        {
            // Commit current transaction and apply new settings immediately
            stageTransactionSettings(isolation_level, read_only, wait_for_locks_,
                                     lock_timeout_seconds_, effective_mode);

            return commit(ctx);
        }
        else
        {
            // Stage settings for next commit/rollback
            stageTransactionSettings(isolation_level, read_only, wait_for_locks_,
                                     lock_timeout_seconds_, effective_mode);

            LOG_DEBUG(TRANSACTION, "Staged transaction settings: isolation=%d, read_only=%d",
                      static_cast<int>(isolation_level), read_only);

            return Status::OK;
        }
    }

    void ConnectionContext::stageTransactionSettings(IsolationLevel isolation_level,
                                                     bool read_only,
                                                     bool wait_for_locks,
                                                     uint32_t lock_timeout_seconds,
                                                     ReadCommittedMode read_committed_mode)
    {
        next_isolation_level_ = isolation_level;
        next_is_read_only_ = read_only;
        next_wait_for_locks_ = wait_for_locks;
        next_lock_timeout_seconds_ = lock_timeout_seconds;
        next_read_committed_mode_ = read_committed_mode;
        settings_staged_ = true;
    }

    void ConnectionContext::stageDefaultTransactionSettings()
    {
        stageTransactionSettings(default_isolation_level_,
                                 default_is_read_only_,
                                 default_wait_for_locks_,
                                 default_lock_timeout_seconds_,
                                 default_read_committed_mode_);
    }

    Status ConnectionContext::beginStatementTracking(const std::string& sql, ErrorContext *ctx)
    {
        clearStatementRestartState();
        // Record the SQL text so dormant reattach can show what was running.
        last_statement_text_ = sql;
        last_statement_hash_ = fnv1a64(sql);
        last_statement_time_ = nowMicros();
        last_activity_time_ = last_statement_time_;
        last_rows_affected_ = 0;
        last_error_code_ = 0;
        last_sqlstate_.clear();
        statement_io_stats_.reset();
        statement_io_active_ = true;
        statement_id_ = last_statement_time_;

        last_statement_line_ = 0;
        last_statement_column_ = 0;
        int32_t line = 1;
        int32_t col = 1;
        bool found = false;
        for (size_t i = 0; i < sql.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(sql[i]);
            if (c == '\n')
            {
                ++line;
                col = 1;
                continue;
            }
            if (!std::isspace(c))
            {
                found = true;
                break;
            }
            ++col;
        }
        if (found)
        {
            last_statement_line_ = line;
            last_statement_column_ = col;
        }

        // Classify statement type using the leading keyword only (fast, dialect-agnostic).
        size_t i = 0;
        while (i < sql.size() && std::isspace(static_cast<unsigned char>(sql[i])))
        {
            ++i;
        }

        std::string keyword;
        for (; i < sql.size(); ++i)
        {
            unsigned char c = static_cast<unsigned char>(sql[i]);
            if (std::isalnum(c) || c == '_' || c == '$')
            {
                keyword.push_back(static_cast<char>(std::toupper(c)));
            }
            else
            {
                break;
            }
        }

        last_statement_query_type_ = classifyQueryType(keyword);

        if (keyword.empty())
        {
            last_statement_type_ = StatementType::UNKNOWN;
        }
        else if (keyword == "SELECT" || keyword == "INSERT" || keyword == "UPDATE" ||
                 keyword == "DELETE" || keyword == "MERGE" || keyword == "WITH")
        {
            last_statement_type_ = StatementType::DML;
        }
        else if (keyword == "CREATE" || keyword == "ALTER" || keyword == "DROP" ||
                 keyword == "TRUNCATE" || keyword == "COMMENT" || keyword == "GRANT" ||
                 keyword == "REVOKE" || keyword == "RENAME")
        {
            last_statement_type_ = StatementType::DDL;
        }
        else
        {
            last_statement_type_ = StatementType::OTHER;
        }

        last_statement_status_ = StatementStatus::IN_PROGRESS;

        if (isolation_level_ == IsolationLevel::READ_COMMITTED_READ_CONSISTENCY &&
            !isForensicReplayActive())
        {
            Status snapshot_status = createStatementXID(ctx);
            if (snapshot_status != Status::OK)
            {
                statement_io_active_ = false;
                statement_id_ = 0;
                last_statement_status_ = StatementStatus::FAILED;
                return snapshot_status;
            }

            Status marker_status = txn_manager_->updateTransactionMarkers(ctx);
            if (marker_status != Status::OK)
            {
                statement_io_active_ = false;
                statement_id_ = 0;
                last_statement_status_ = StatementStatus::FAILED;
                clearStatementXID(nullptr);
                return marker_status;
            }
        }

        auto& metrics = ScratchBirdMetrics::getInstance();
        metrics.initialize();
        if (metrics.query_currently_running)
        {
            std::string database_name = "default";
            if (db_)
            {
                database_name = baseNameFromPath(db_->path());
                if (database_name.empty())
                {
                    database_name = "default";
                }
            }
            metrics.query_currently_running->inc(1.0, {database_name});
        }

        ProcArrayManager::setQueryInfo(proc_id_, last_statement_time_, sql, nullptr);
        return Status::OK;
    }

    void ConnectionContext::endStatementTrackingSuccess(int64_t rows_affected)
    {
        Status clear_status = clearStatementXID(nullptr);
        if (clear_status != Status::OK && clear_status != Status::INVALID_ARGUMENT)
        {
            LOG_WARNING(TRANSACTION, "Failed to clear statement snapshot for proc_id %u", proc_id_);
        }
        Status marker_status = txn_manager_->updateTransactionMarkers(nullptr);
        if (marker_status != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to update transaction markers after statement success");
        }

        uint64_t start_time = last_statement_time_;
        uint64_t end_time = nowMicros();
        last_statement_status_ = StatementStatus::COMPLETED;
        last_rows_affected_ = rows_affected;
        last_error_code_ = 0;
        last_sqlstate_.clear();
        last_statement_time_ = end_time;
        last_activity_time_ = last_statement_time_;
        ProcArrayManager::clearQueryInfo(proc_id_, last_statement_time_, nullptr);
        statement_io_active_ = false;
        statement_id_ = 0;

        std::string database_name = "default";
        if (db_)
        {
            database_name = baseNameFromPath(db_->path());
            if (database_name.empty())
            {
                database_name = "default";
            }
        }

        std::string query_type = last_statement_query_type_.empty()
            ? "unknown"
            : last_statement_query_type_;

        auto& metrics = ScratchBirdMetrics::getInstance();
        metrics.initialize();
        if (metrics.queries_total)
        {
            metrics.queries_total->inc(1.0, {query_type, database_name});
        }
        if (metrics.query_currently_running)
        {
            metrics.query_currently_running->dec(1.0, {database_name});
        }
        if (metrics.query_duration_seconds && start_time != 0 && end_time >= start_time)
        {
            double duration_seconds =
                static_cast<double>(end_time - start_time) / 1000000.0;
            metrics.query_duration_seconds->observe(duration_seconds,
                                                    {query_type, database_name});
        }
        if (metrics.query_rows_returned_total &&
            query_type == "select" &&
            rows_affected > 0)
        {
            metrics.query_rows_returned_total->inc(
                static_cast<double>(rows_affected),
                {"select", database_name});
        }
        if (metrics.query_rows_affected_total &&
            rows_affected > 0 &&
            (query_type == "insert" || query_type == "update" || query_type == "delete"))
        {
            metrics.query_rows_affected_total->inc(
                static_cast<double>(rows_affected),
                {query_type, database_name});
        }

        CatalogManager *catalog = db_ ? db_->catalog_manager() : nullptr;
        if (catalog && !last_statement_text_.empty())
        {
            std::string digest_text = normalizeDigestText(last_statement_text_);
            if (!digest_text.empty())
            {
                uint64_t digest_hash = fnv1a64(digest_text);
                CatalogManager::StatementDigestEntry entry;
                entry.schema_name = current_schema_name_;
                CatalogManager::SessionInfo session_info;
                if (!isZeroUuidLocal(session_id_) &&
                    catalog->getSession(session_id_, session_info, nullptr) == Status::OK)
                {
                    entry.user_name = session_info.username;
                }
                if (entry.user_name.empty())
                {
                    entry.user_name = "unknown";
                }
                entry.host_name = "local";
                entry.digest = digestHex(digest_hash);
                entry.digest_text = digest_text;
                entry.count_star = 1;
                entry.sum_timer_wait = (start_time != 0 && end_time >= start_time)
                    ? (end_time - start_time)
                    : 0;
                entry.min_timer_wait = entry.sum_timer_wait;
                entry.max_timer_wait = entry.sum_timer_wait;
                entry.sum_rows_affected = rows_affected > 0 ? static_cast<uint64_t>(rows_affected) : 0;
                entry.first_seen = end_time;
                entry.last_seen = end_time;
                entry.query_sample_text = last_statement_text_;
                entry.query_sample_seen = end_time;
                entry.query_sample_timer_wait = entry.sum_timer_wait;
                catalog->recordStatementDigest(entry, nullptr);
            }
        }
    }

    void ConnectionContext::updateStatementSourceLocation(int32_t line, int32_t column)
    {
        if (line <= 0 || column <= 0)
        {
            return;
        }
        last_statement_line_ = line;
        last_statement_column_ = column;
        last_activity_time_ = nowMicros();
    }

    void ConnectionContext::endStatementTrackingFailure(uint32_t error_code,
                                                       const std::string& sqlstate)
    {
        Status clear_status = clearStatementXID(nullptr);
        if (clear_status != Status::OK && clear_status != Status::INVALID_ARGUMENT)
        {
            LOG_WARNING(TRANSACTION, "Failed to clear statement snapshot for proc_id %u", proc_id_);
        }
        Status marker_status = txn_manager_->updateTransactionMarkers(nullptr);
        if (marker_status != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to update transaction markers after statement failure");
        }

        uint64_t start_time = last_statement_time_;
        uint64_t end_time = nowMicros();
        last_statement_status_ = StatementStatus::FAILED;
        last_rows_affected_ = 0;
        last_error_code_ = error_code;
        last_sqlstate_ = sqlstate;
        last_statement_time_ = end_time;
        last_activity_time_ = last_statement_time_;
        ProcArrayManager::clearQueryInfo(proc_id_, last_statement_time_, nullptr);
        statement_io_active_ = false;
        statement_id_ = 0;

        std::string database_name = "default";
        if (db_)
        {
            database_name = baseNameFromPath(db_->path());
            if (database_name.empty())
            {
                database_name = "default";
            }
        }

        std::string query_type = last_statement_query_type_.empty()
            ? "unknown"
            : last_statement_query_type_;

        auto& metrics = ScratchBirdMetrics::getInstance();
        metrics.initialize();
        if (metrics.queries_total)
        {
            metrics.queries_total->inc(1.0, {query_type, database_name});
        }
        if (metrics.query_currently_running)
        {
            metrics.query_currently_running->dec(1.0, {database_name});
        }
        if (metrics.query_duration_seconds && start_time != 0 && end_time >= start_time)
        {
            double duration_seconds =
                static_cast<double>(end_time - start_time) / 1000000.0;
            metrics.query_duration_seconds->observe(duration_seconds,
                                                    {query_type, database_name});
        }
        if (metrics.query_errors_total)
        {
            metrics.query_errors_total->inc(1.0, {"error", database_name});
        }

        CatalogManager *catalog = db_ ? db_->catalog_manager() : nullptr;
        if (catalog && !last_statement_text_.empty())
        {
            std::string digest_text = normalizeDigestText(last_statement_text_);
            if (!digest_text.empty())
            {
                uint64_t digest_hash = fnv1a64(digest_text);
                CatalogManager::StatementDigestEntry entry;
                entry.schema_name = current_schema_name_;
                CatalogManager::SessionInfo session_info;
                if (!isZeroUuidLocal(session_id_) &&
                    catalog->getSession(session_id_, session_info, nullptr) == Status::OK)
                {
                    entry.user_name = session_info.username;
                }
                if (entry.user_name.empty())
                {
                    entry.user_name = "unknown";
                }
                entry.host_name = "local";
                entry.digest = digestHex(digest_hash);
                entry.digest_text = digest_text;
                entry.count_star = 1;
                entry.sum_timer_wait = (start_time != 0 && end_time >= start_time)
                    ? (end_time - start_time)
                    : 0;
                entry.min_timer_wait = entry.sum_timer_wait;
                entry.max_timer_wait = entry.sum_timer_wait;
                entry.sum_errors = 1;
                entry.first_seen = end_time;
                entry.last_seen = end_time;
                entry.query_sample_text = last_statement_text_;
                entry.query_sample_seen = end_time;
                entry.query_sample_timer_wait = entry.sum_timer_wait;
                catalog->recordStatementDigest(entry, nullptr);
            }
        }
    }

    void ConnectionContext::recordPageRead()
    {
        connection_io_stats_.recordRead();
        transaction_io_stats_.recordRead();
        if (statement_io_active_)
        {
            statement_io_stats_.recordRead();
        }
    }

    void ConnectionContext::recordPageWrite()
    {
        connection_io_stats_.recordWrite();
        transaction_io_stats_.recordWrite();
        if (statement_io_active_)
        {
            statement_io_stats_.recordWrite();
        }
    }

    void ConnectionContext::recordPageFetch()
    {
        connection_io_stats_.recordFetch();
        transaction_io_stats_.recordFetch();
        if (statement_io_active_)
        {
            statement_io_stats_.recordFetch();
        }
    }

    void ConnectionContext::recordPageMark()
    {
        connection_io_stats_.recordMark();
        transaction_io_stats_.recordMark();
        if (statement_io_active_)
        {
            statement_io_stats_.recordMark();
        }
    }

    IOStatsSnapshot ConnectionContext::snapshotConnectionIoStats() const
    {
        return connection_io_stats_.snapshot();
    }

    IOStatsSnapshot ConnectionContext::snapshotTransactionIoStats() const
    {
        return transaction_io_stats_.snapshot();
    }

    IOStatsSnapshot ConnectionContext::snapshotStatementIoStats() const
    {
        return statement_io_stats_.snapshot();
    }

    void ConnectionContext::recordTableDmlDelta(const ID& table_id,
                                               uint64_t inserts,
                                               uint64_t updates,
                                               uint64_t deletes,
                                               uint64_t hot_updates,
                                               uint64_t newpage_updates)
    {
        if (isZeroUuidLocal(table_id))
        {
            return;
        }

        TableDmlDelta& delta = pending_table_deltas_[table_id];
        delta.inserts += inserts;
        delta.updates += updates;
        delta.deletes += deletes;
        delta.hot_updates += hot_updates;
        delta.newpage_updates += newpage_updates;
    }

    std::string ConnectionContext::sessionSettingsJson() const
    {
        // Stable ordering keeps diffs readable in dormant transaction records.
        std::string json;
        json.reserve(256);

        json.append("{\"search_path\":[");
        for (size_t i = 0; i < search_path_.size(); ++i)
        {
            if (i > 0)
            {
                json.push_back(',');
            }
            appendJsonString(json, search_path_[i]);
        }
        json.append("],\"dialect_tag\":");
        appendJsonString(json, dialect_tag_);
        json.append(",\"sql_dialect\":");
        json.append(std::to_string(sql_dialect_));
        json.append(",\"charset\":");
        appendJsonString(json, charset_);
        json.append(",\"statement_timeout\":");
        json.append(std::to_string(statement_timeout_seconds_));
        json.push_back('}');

        return json;
    }

    Status ConnectionContext::reserveTables(const std::vector<TableReservation> &reservations,
                                            ErrorContext *ctx)
    {
        // Resolve table names to UUIDs so renames don't affect lock identity.
        table_reservations_.clear();
        table_reservations_.reserve(reservations.size());

        CatalogManager *catalog = db_ ? db_->catalog_manager() : nullptr;
        if (!catalog)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "CatalogManager not available");
            return Status::INVALID_ARGUMENT;
        }

        for (const auto &reservation : reservations)
        {
            TableReservation resolved = reservation;
            if (isZeroUuidLocal(resolved.table_id))
            {
                if (resolved.table_name.empty())
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT, "Table name required");
                    return Status::INVALID_ARGUMENT;
                }

                ObjectPath path;
                path.type = PathType::UNQUALIFIED;

                size_t start = 0;
                while (start < resolved.table_name.size())
                {
                    size_t dot = resolved.table_name.find('.', start);
                    if (dot == std::string::npos)
                    {
                        path.components.push_back(resolved.table_name.substr(start));
                        break;
                    }
                    if (dot == start)
                    {
                        SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                          "Invalid table name in RESERVING clause");
                        return Status::INVALID_ARGUMENT;
                    }
                    path.components.push_back(resolved.table_name.substr(start, dot - start));
                    start = dot + 1;
                }

                if (path.components.empty())
                {
                    SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                      "Invalid table name in RESERVING clause");
                    return Status::INVALID_ARGUMENT;
                }

                if (path.components.size() > 1)
                {
                    path.type = PathType::ABSOLUTE;
                }

                CatalogManager::ResolveOptions opts;
                opts.dialect_tag = dialect_tag_;

                ID resolved_id;
                CatalogManager::ObjectType resolved_type;
                Status status = catalog->resolveObjectPath(
                    path, CatalogManager::ObjectType::TABLE, opts,
                    resolved_id, resolved_type, ctx);
                if (status != Status::OK)
                {
                    std::string msg = "Failed to resolve table '" + resolved.table_name + "'";
                    SET_ERROR_CONTEXT(ctx, status, msg.c_str());
                    return status;
                }

                (void)resolved_type;
                resolved.table_id = resolved_id;
            }

            table_reservations_.push_back(std::move(resolved));
        }

        LOG_DEBUG(LOCK, "Reserved %zu tables for transaction: proc_id=%u, xid=%lu",
                  table_reservations_.size(), proc_id_, current_xid_);

        // Note: Table locks will be acquired when the next transaction starts with
        // SNAPSHOT TABLE STABILITY isolation level. See beginNewTransaction().

        return Status::OK;
    }

    Status ConnectionContext::beginNewTransaction(ErrorContext *ctx)
    {
        pending_transactional_ddl_batches_.clear();
        // Allocate new XID
        uint64_t new_xid = 0;
        Status s = txn_manager_->beginTransaction(proc_id_, new_xid, ctx);
        if (s != Status::OK)
        {
            return s;
        }

        // Update context
        current_xid_ = new_xid;
        xact_start_time_ = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now().time_since_epoch());
        // Track activity at transaction start for dormant transaction auditing.
        last_activity_time_ = static_cast<uint64_t>(xact_start_time_.count());
        transaction_io_stats_.reset();
        statement_io_stats_.reset();
        statement_io_active_ = false;
        statement_id_ = 0;
        pending_table_deltas_.clear();

        // Update isolation level in ProcArray for transaction marker tracking
        s = ProcArrayManager::setIsolationLevel(proc_id_, static_cast<uint8_t>(isolation_level_),
                                                ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to set isolation level in ProcArray for proc_id %u",
                        proc_id_);
            // Non-fatal - continue with transaction
        }

        // Update read-only flag in ProcArray for long transaction monitoring
        s = ProcArrayManager::setTransactionReadOnly(proc_id_, is_read_only_, ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to set read-only flag in ProcArray for proc_id %u",
                        proc_id_);
            // Non-fatal - continue with transaction
        }

        // Update transaction start time in ProcArray for long transaction monitoring
        s = ProcArrayManager::setTransactionStartTime(proc_id_, xact_start_time_.count(), ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION,
                        "Failed to set transaction start time in ProcArray for proc_id %u",
                        proc_id_);
            // Non-fatal - continue with transaction
        }

        // Capture a retained transaction-start snapshot only for ordinary snapshot-style
        // transactions. Replay-bound sessions already have an immutable retained boundary.
        if (!isForensicReplayActive() &&
            (isolation_level_ == IsolationLevel::SNAPSHOT ||
             isolation_level_ == IsolationLevel::SNAPSHOT_TABLE_STABILITY))
        {
            s = createSnapshot(ctx);
            if (s != Status::OK)
            {
                // Failed to create snapshot - rollback the transaction
                txn_manager_->rollbackTransaction(proc_id_, current_xid_, nullptr);
                current_xid_ = 0;
                return s;
            }
        }
        else
        {
            Status clear_pin_status = txn_manager_->clearSnapshotPin(proc_id_, nullptr);
            if (clear_pin_status != Status::OK && clear_pin_status != Status::INVALID_ARGUMENT)
            {
                LOG_WARNING(TRANSACTION, "Failed to clear snapshot pin for proc_id %u", proc_id_);
            }
        }

        // Update transaction markers (OAT, OST) after snapshot publication is complete.
        s = txn_manager_->updateTransactionMarkers(ctx);
        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to update transaction markers");
            // Non-fatal - continue with transaction
        }
        transaction_start_oit_ = txn_manager_->getOldestXid();
        transaction_start_oat_ = txn_manager_->getOldestActiveXid();
        transaction_start_ost_ = txn_manager_->getOldestSnapshot();

        // Acquire table locks if using SNAPSHOT TABLE STABILITY
        if (isolation_level_ == IsolationLevel::SNAPSHOT_TABLE_STABILITY &&
            !table_reservations_.empty())
        {
            LockManager *lock_mgr = db_->lock_manager();
            if (!lock_mgr)
            {
                LOG_ERROR(LOCK, "LockManager not available");
                return Status::IO_ERROR;
            }

            // Acquire locks for each reserved table
            for (const auto &reservation : table_reservations_)
            {
                if (isZeroUuidLocal(reservation.table_id))
                {
                    LOG_ERROR(LOCK, "Missing table UUID for reservation '%s'",
                              reservation.table_name.c_str());
                    lock_mgr->releaseAllLocks(proc_id_, nullptr);
                    return Status::INVALID_ARGUMENT;
                }

                // Convert TableLockMode to LockMode
                LockMode lock_mode;
                if (reservation.lock_mode == TableLockMode::SHARED)
                {
                    // SHARED allows concurrent reads
                    lock_mode = LockMode::LOCK_SHARE;
                }
                else // PROTECTED
                {
                    // PROTECTED gives exclusive access
                    lock_mode = LockMode::LOCK_ACCESS_EXCLUSIVE;
                }

                // Create lock tag for table
                LockTag tag;
                tag.target_type = LockTarget::LOCK_TARGET_TABLE;
                tag.object_uuid = reservation.table_id;
                tag.page_num = 0;
                tag.offset_num = 0;
                tag.padding = 0;

                // Acquire the lock
                uint32_t timeout_ms = lock_timeout_seconds_ * 1000;
                s = lock_mgr->acquireLock(proc_id_, tag, lock_mode, wait_for_locks_, timeout_ms,
                                          ctx);
                if (s != Status::OK)
                {
                    LOG_ERROR(LOCK, "Failed to acquire %s lock on table '%s'",
                              reservation.lock_mode == TableLockMode::SHARED ? "SHARED"
                                                                             : "PROTECTED",
                              reservation.table_name.c_str());
                    // Release all locks acquired so far
                    lock_mgr->releaseAllLocks(proc_id_, nullptr);
                    return s;
                }

                LOG_DEBUG(LOCK,
                          "Acquired %s lock on table '%s' for transaction: proc_id=%u, xid=%lu",
                          reservation.lock_mode == TableLockMode::SHARED ? "SHARED" : "PROTECTED",
                          reservation.table_name.c_str(), proc_id_, current_xid_);
            }
        }

        current_transaction_uuid_ = generateUuidV7();
        last_commit_seqno_ = 0;
        std::memset(&lineage_root_event_id_, 0, sizeof(lineage_root_event_id_));
        s = ensureCurrentSchemaEpochInitialized(ctx);
        if (s != Status::OK)
        {
            txn_manager_->rollbackTransaction(proc_id_, current_xid_, nullptr);
            current_xid_ = 0;
            std::memset(&current_transaction_uuid_, 0, sizeof(current_transaction_uuid_));
            return s;
        }
        transaction_start_schema_epoch_uuid_ = current_schema_epoch_uuid_;

        auto rollback_begin = [this]() {
            if (db_)
            {
                if (auto* catalog = db_->catalog_manager())
                {
                    ErrorContext cleanup_ctx;
                    catalog->deleteRuntimeTransactionCatalogEntry(current_xid_, &cleanup_ctx);
                }
                if (auto* lock_mgr = db_->lock_manager())
                {
                    ErrorContext cleanup_ctx;
                    lock_mgr->releaseAllLocks(proc_id_, &cleanup_ctx);
                }
            }
            txn_manager_->rollbackTransaction(proc_id_, current_xid_, nullptr);
            current_xid_ = 0;
            std::memset(&current_transaction_uuid_, 0, sizeof(current_transaction_uuid_));
            std::memset(&lineage_root_event_id_, 0, sizeof(lineage_root_event_id_));
            xact_start_time_ = std::chrono::microseconds(0);
            transaction_start_oit_ = 0;
            transaction_start_oat_ = 0;
            transaction_start_ost_ = 0;
            retained_transaction_snapshot_.reset();
            pending_transactional_ddl_batches_.clear();
        };

        s = persistRuntimeTransactionState(
            static_cast<uint8_t>(CatalogManager::RuntimeTransactionState::IN_PROGRESS),
            current_xid_,
            current_transaction_uuid_,
            forensic_replay_binding_ ? forensic_replay_binding_->capsule_uuid : ID{},
            current_schema_epoch_uuid_,
            false,
            0,
            0,
            ctx);
        if (s != Status::OK)
        {
            rollback_begin();
            return s;
        }

        s = appendTransactionLineageBegin(ctx);
        if (s != Status::OK)
        {
            rollback_begin();
            return s;
        }

        return Status::OK;
    }

    Status ConnectionContext::endCurrentTransaction(bool commit, ErrorContext *ctx)
    {
        const uint64_t ended_xid = current_xid_;
        const ID ended_tx_uuid = current_transaction_uuid_;
        const uint64_t start_time = static_cast<uint64_t>(xact_start_time_.count());
        const bool persist_tx_evidence = shouldPersistTransactionEvidence();
        uint64_t terminal_commit_seqno = 0;
        Status s;

        if (commit)
        {
            uint64_t committed_seqno = 0;
            s = txn_manager_->commitTransactionWithSequence(
                proc_id_, current_xid_, committed_seqno, ctx);
            if (s == Status::OK)
            {
                terminal_commit_seqno = committed_seqno;
            }
        }
        else
        {
            s = txn_manager_->rollbackTransaction(proc_id_, current_xid_, ctx);
        }

        // Release all locks held by this transaction
        LockManager *lock_mgr = db_->lock_manager();
        if (lock_mgr)
        {
            Status lock_status = lock_mgr->releaseAllLocks(proc_id_, ctx);
            if (lock_status != Status::OK)
            {
                LOG_WARNING(LOCK, "Failed to release locks after %s: proc_id=%u, xid=%lu",
                            commit ? "commit" : "rollback", proc_id_, current_xid_);
                // Non-fatal - continue with transaction cleanup
            }
            else
            {
                LOG_DEBUG(LOCK, "Released all locks for transaction: proc_id=%u, xid=%lu", proc_id_,
                          current_xid_);
            }
        }

        if (s == Status::OK)
        {
            CatalogManager *catalog = db_ ? db_->catalog_manager() : nullptr;
            const uint64_t end_time = nowMicros();
            uint64_t commit_seqno = 0;
            bool has_commit_seqno = false;
            if (commit && !isForensicReplayActive() && persist_tx_evidence)
            {
                commit_seqno = terminal_commit_seqno;
                if (commit_seqno != 0)
                {
                    has_commit_seqno = true;
                }
                else
                {
                    ErrorContext commit_seq_ctx;
                    Status commit_seq_status = txn_manager_->getCommittedTransactionSequence(
                        ended_xid, commit_seqno, &commit_seq_ctx);
                    if (commit_seq_status == Status::OK && commit_seqno != 0)
                    {
                        has_commit_seqno = true;
                    }
                    else
                    {
                        LOG_WARNING(TRANSACTION,
                                    "Failed to resolve durable commit sequence after commit: proc_id=%u, xid=%lu, status=%d, detail=%s",
                                    proc_id_,
                                    ended_xid,
                                    static_cast<int>(commit_seq_status),
                                    commit_seq_ctx.message.c_str());
                    }
                }
            }
            if (commit && !isForensicReplayActive() && persist_tx_evidence)
            {
                Status ddl_status = flushCommittedTransactionalDdl(
                    has_commit_seqno ? commit_seqno : 0, nullptr);
                if (ddl_status != Status::OK)
                {
                    LOG_WARNING(TRANSACTION,
                                "Failed to persist transactional DDL lineage/schema epoch: proc_id=%u, xid=%lu, status=%d",
                                proc_id_, ended_xid, static_cast<int>(ddl_status));
                }
            }
            ID terminal_capsule_uuid =
                forensic_replay_binding_ ? forensic_replay_binding_->capsule_uuid : ID{};
            ID terminal_schema_epoch_uuid = current_schema_epoch_uuid_;
            if (!isForensicReplayActive())
            {
                Status capsule_status = createForensicSnapshotCapsule(
                    commit,
                    ended_xid,
                    ended_tx_uuid,
                    has_commit_seqno ? commit_seqno : 0,
                    end_time,
                    terminal_capsule_uuid,
                    nullptr);
                if (capsule_status != Status::OK)
                {
                    LOG_WARNING(TRANSACTION,
                                "Failed to materialize forensic snapshot capsule: proc_id=%u, xid=%lu, status=%d",
                                proc_id_, ended_xid, static_cast<int>(capsule_status));
                }
            }
            Status runtime_status = persistRuntimeTransactionState(
                static_cast<uint8_t>(commit ? CatalogManager::RuntimeTransactionState::COMMITTED
                                            : CatalogManager::RuntimeTransactionState::ABORTED),
                ended_xid,
                ended_tx_uuid,
                terminal_capsule_uuid,
                terminal_schema_epoch_uuid,
                commit && !isForensicReplayActive() && has_commit_seqno,
                commit && !isForensicReplayActive() && has_commit_seqno ? commit_seqno : 0,
                end_time,
                nullptr);
            if (runtime_status != Status::OK)
            {
                LOG_WARNING(TRANSACTION,
                            "Failed to persist terminal runtime transaction row: proc_id=%u, xid=%lu, status=%d",
                            proc_id_, ended_xid, static_cast<int>(runtime_status));
            }

            ErrorContext lineage_ctx;
            Status lineage_status = appendTransactionLineageTerminal(
                commit, ended_xid, ended_tx_uuid, start_time, end_time, &lineage_ctx);
            if (lineage_status != Status::OK)
            {
                LOG_WARNING(TRANSACTION,
                            "Failed to persist terminal transaction lineage: proc_id=%u, xid=%lu, status=%d, detail=%s",
                            proc_id_, ended_xid, static_cast<int>(lineage_status),
                            lineage_ctx.message.c_str());
            }

            if (catalog)
            {
                CatalogManager::TransactionHistoryEntry entry;
                entry.thread_id = proc_id_;
                entry.trx_id = ended_xid;
                entry.start_oit = transaction_start_oit_;
                entry.end_oit = txn_manager_->getOldestXid();
                entry.start_oat = transaction_start_oat_;
                entry.end_oat = txn_manager_->getOldestActiveXid();
                entry.start_ost = transaction_start_ost_;
                entry.end_ost = txn_manager_->getOldestSnapshot();
                entry.timer_start = start_time;
                entry.timer_end = end_time;
                entry.timer_wait = (start_time != 0 && end_time >= start_time) ? (end_time - start_time) : 0;
                entry.read_only = is_read_only_;
                entry.isolation_level = static_cast<uint8_t>(isolation_level_);
                entry.autocommit = autocommit_mode_ && !autocommit_suspended_;
                entry.committed = commit;
                entry.event_id = start_time != 0 ? start_time : ended_xid;
                entry.end_event_id = end_time;
                catalog->recordTransactionHistory(entry, nullptr);
            }
        }

        if (s == Status::OK && commit && db_ && !pending_table_deltas_.empty())
        {
            if (auto* stats_mgr = db_->table_stats_manager())
            {
                for (const auto& [table_id, delta] : pending_table_deltas_)
                {
                    stats_mgr->applyCommittedDelta(table_id, delta);
                }
            }
        }
        pending_table_deltas_.clear();
        pending_transactional_ddl_batches_.clear();
        transaction_io_stats_.reset();
        statement_io_stats_.reset();
        statement_io_active_ = false;
        statement_id_ = 0;
        clearStatementRestartState();
        if (!commit)
        {
            current_schema_epoch_uuid_ = transaction_start_schema_epoch_uuid_;
        }

        Status clear_statement_status = clearStatementXID(nullptr);
        if (clear_statement_status != Status::OK &&
            clear_statement_status != Status::INVALID_ARGUMENT)
        {
            LOG_WARNING(TRANSACTION, "Failed to clear statement snapshot for proc_id %u", proc_id_);
        }

        // Clear savepoint stack (transaction ending clears all savepoints)
        savepoint_stack_.clear();
        savepoint_level_ = 0;
        command_id_ = 0;
        retained_transaction_snapshot_.reset();
        Status clear_snapshot_status = txn_manager_->clearSnapshotPin(proc_id_, nullptr);
        if (clear_snapshot_status != Status::OK &&
            clear_snapshot_status != Status::INVALID_ARGUMENT)
        {
            LOG_WARNING(TRANSACTION, "Failed to clear retained snapshot pin for proc_id %u", proc_id_);
        }

        // Clear transaction state (will be reset by beginNewTransaction)
        std::memset(&current_transaction_uuid_, 0, sizeof(current_transaction_uuid_));
        std::memset(&lineage_root_event_id_, 0, sizeof(lineage_root_event_id_));
        std::memset(&transaction_start_schema_epoch_uuid_, 0, sizeof(transaction_start_schema_epoch_uuid_));
        current_xid_ = 0;
        transaction_start_oit_ = 0;
        transaction_start_oat_ = 0;
        transaction_start_ost_ = 0;
        xact_start_time_ = std::chrono::microseconds(0);

        // Update transaction markers (OAT, OST) after ending transaction
        Status marker_status = txn_manager_->updateTransactionMarkers(ctx);
        if (marker_status != Status::OK)
        {
            LOG_WARNING(TRANSACTION, "Failed to update transaction markers after %s",
                        commit ? "commit" : "rollback");
            // Non-fatal - markers will be updated on next transaction
        }

        return s;
    }

    void ConnectionContext::applyStagedSecurityContext()
    {
        if (!security_context_staged_)
        {
            return;
        }

        if (pending_user_change_)
        {
            current_user_id_ = pending_user_id_;
            is_superuser_ = pending_is_superuser_;
            std::memset(&active_role_id_, 0, sizeof(active_role_id_));
            pending_user_change_ = false;
        }

        if (pending_role_change_)
        {
            active_role_id_ = pending_role_id_;
            pending_role_change_ = false;
        }

        if (pending_session_change_)
        {
            session_id_ = pending_session_id_;
            authkey_id_ = pending_authkey_id_;
            emulation_mode_ = pending_emulation_mode_.empty()
                                  ? emulation_mode_
                                  : pending_emulation_mode_;
            policy_epoch_global_ = pending_policy_epoch_global_;
            policy_epoch_table_ = pending_policy_epoch_table_;
            pending_session_change_ = false;
            pending_emulation_mode_.clear();
            Status s = ProcArrayManager::setSessionId(proc_id_, session_id_, nullptr);
            if (s != Status::OK)
            {
                LOG_WARNING(TRANSACTION,
                            "Failed to set staged session id in ProcArray for proc_id %u",
                            proc_id_);
            }
        }

        security_context_staged_ = false;

        LOG_DEBUG(TRANSACTION, "Applied staged security context changes: proc_id=%u", proc_id_);
    }

    void ConnectionContext::applyStagedSettings()
    {
        if (settings_staged_)
        {
            isolation_level_ = next_isolation_level_;
            is_read_only_ = next_is_read_only_;
            wait_for_locks_ = next_wait_for_locks_;
            lock_timeout_seconds_ = next_lock_timeout_seconds_;
            read_committed_mode_ = next_read_committed_mode_;
            settings_staged_ = false;

            LOG_DEBUG(TRANSACTION,
                      "Applied staged settings: isolation=%d, read_only=%d, wait=%d, lock_timeout=%u, rc_mode=%d",
                      static_cast<int>(isolation_level_), is_read_only_, wait_for_locks_,
                      lock_timeout_seconds_, static_cast<int>(read_committed_mode_));
        }

        if (statement_timeout_local_override_)
        {
            statement_timeout_seconds_ = default_statement_timeout_seconds_;
            statement_timeout_local_override_ = false;
        }

        applyStagedSecurityContext();
    }

    Status ConnectionContext::createSnapshot(ErrorContext *ctx)
    {
        retained_transaction_snapshot_ = std::make_unique<TransactionSnapshot>();
        Status status = txn_manager_->captureSnapshot(*retained_transaction_snapshot_, ctx);
        if (status != Status::OK)
        {
            retained_transaction_snapshot_.reset();
            return status;
        }
        retained_transaction_snapshot_->snapshot_txid_low =
            std::min(retained_transaction_snapshot_->snapshot_txid_low == 0
                         ? current_xid_
                         : retained_transaction_snapshot_->snapshot_txid_low,
                     current_xid_);
        status = txn_manager_->registerSnapshotPin(proc_id_, current_xid_,
                                                   *retained_transaction_snapshot_, ctx);
        if (status != Status::OK)
        {
            retained_transaction_snapshot_.reset();
            return status;
        }
        LOG_DEBUG(TRANSACTION,
                  "Captured retained transaction snapshot: proc_id=%u, xid=%lu, xmin=%lu, xmax=%lu, active=%zu",
                  proc_id_,
                  current_xid_,
                  retained_transaction_snapshot_->snapshot_txid_low,
                  retained_transaction_snapshot_->snapshot_txid_high,
                  retained_transaction_snapshot_->active_txid_set.size());
        return Status::OK;
    }

    Status ConnectionContext::createStatementXID(ErrorContext *ctx)
    {
        statement_transaction_snapshot_.reset();
        statement_xid_ = 0;
        if (current_xid_ != 0)
        {
            statement_transaction_snapshot_ = std::make_unique<TransactionSnapshot>();
            Status status = txn_manager_->captureSnapshot(*statement_transaction_snapshot_, ctx);
            if (status != Status::OK)
            {
                statement_transaction_snapshot_.reset();
                return status;
            }
            statement_transaction_snapshot_->snapshot_txid_low =
                std::min(statement_transaction_snapshot_->snapshot_txid_low == 0
                             ? current_xid_
                             : statement_transaction_snapshot_->snapshot_txid_low,
                         current_xid_);
            status = txn_manager_->registerSnapshotPin(proc_id_, current_xid_,
                                                       *statement_transaction_snapshot_, ctx);
            if (status != Status::OK)
            {
                statement_transaction_snapshot_.reset();
                return status;
            }
            statement_xid_ = current_xid_;
            LOG_DEBUG(TRANSACTION,
                      "Created statement snapshot: proc_id=%u, xid=%lu, xmin=%lu, xmax=%lu",
                      proc_id_,
                      statement_xid_,
                      statement_transaction_snapshot_->snapshot_txid_low,
                      statement_transaction_snapshot_->snapshot_txid_high);
        }
        return Status::OK;
    }

    void ConnectionContext::clearStatementRestartState()
    {
        last_statement_restart_decision_ = StatementRestartDecision{};
        statement_restart_count_ = 0;
    }

    Status ConnectionContext::clearStatementXID(ErrorContext *ctx)
    {
        if (statement_xid_ != 0)
        {
            Status status = txn_manager_->clearSnapshotPin(proc_id_, ctx);
            if (status != Status::OK && status != Status::INVALID_ARGUMENT)
            {
                return status;
            }
            LOG_DEBUG(TRANSACTION, "Cleared statement XID %lu", statement_xid_);
            statement_xid_ = 0;
        }
        statement_transaction_snapshot_.reset();
        return Status::OK;
    }

    Status ConnectionContext::registerReadConsistencyRestart(
        const StatementRestartDecision& decision, ErrorContext *ctx)
    {
        if (!decision.restart_required)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Read-consistency restart decision is not restart-eligible");
            return Status::INVALID_ARGUMENT;
        }

        last_statement_restart_decision_ = decision;
        if (statement_restart_count_ < std::numeric_limits<uint32_t>::max())
        {
            ++statement_restart_count_;
        }

        const std::string message = TransactionManager::formatStatementRestartMessage(decision);
        SET_ERROR_CONTEXT(ctx, Status::SERIALIZATION_FAILURE, message.c_str());
        return Status::SERIALIZATION_FAILURE;
    }

    Status ConnectionContext::checkTerminationRequested(ErrorContext *ctx)
    {
        // Check if long transaction monitor has requested termination
        bool termination_requested = false;
        Status s =
            ProcArrayManager::isTerminationRequested(proc_id_, &termination_requested, ctx);

        if (s != Status::OK)
        {
            LOG_WARNING(TRANSACTION,
                        "Failed to check termination status for proc_id %u: status=%d", proc_id_,
                        static_cast<int>(s));
            // Non-fatal - continue with transaction
            return Status::OK;
        }

        if (termination_requested)
        {
            LOG_ERROR(TRANSACTION,
                      "Connection termination requested by long transaction monitor: proc_id=%u, "
                      "xid=%lu",
                      proc_id_, current_xid_);

            // Clear the termination request flag
            Status clear_status = ProcArrayManager::clearTerminationRequest(proc_id_, ctx);
            if (clear_status != Status::OK)
            {
                LOG_WARNING(TRANSACTION,
                            "Failed to clear termination request for proc_id %u: status=%d",
                            proc_id_, static_cast<int>(clear_status));
            }

            // Set error context to inform caller
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR,
                              "Connection terminated due to long-running transaction");

            return Status::IO_ERROR;
        }

        return Status::OK;
    }

    // ============================================================================
    // Savepoint/Subtransaction Support (Issue 2.15)
    // ============================================================================

    Status ConnectionContext::createSavepoint(const std::string &name, ErrorContext *ctx)
    {
        if (current_xid_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Cannot create savepoint outside of a transaction");
            return Status::INVALID_ARGUMENT;
        }

        // Check for duplicate savepoint name
        for (const auto &sp : savepoint_stack_)
        {
            if (sp.name == name)
            {
                SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                                  "Savepoint with this name already exists");
                return Status::INVALID_ARGUMENT;
            }
        }

        // Create new savepoint
        Savepoint sp;
        sp.name = name;
        sp.level = ++savepoint_level_;
        sp.xid = current_xid_;
        sp.command_id = command_id_;

        // FIREBIRD MGA: No snapshot needed - XID is sufficient for rollback
        // Savepoint just marks a point in transaction for potential rollback

        // Add to stack
        savepoint_stack_.push_back(std::move(sp));

        LOG_DEBUG(TRANSACTION, "Created savepoint '%s' at level %u: proc_id=%u, xid=%lu",
                  name.c_str(), sp.level, proc_id_, current_xid_);

        return Status::OK;
    }

    auto ConnectionContext::findSavepointBackoutChange(Savepoint &savepoint,
                                                       const ID &table_id,
                                                       uint32_t stable_page_id,
                                                       uint16_t stable_item_id)
        -> SavepointBackoutAction *
    {
        for (auto &change : savepoint.changes)
        {
            if (change.matches(table_id, stable_page_id, stable_item_id))
            {
                return &change;
            }
        }
        return nullptr;
    }

    void ConnectionContext::recordSavepointBackoutAction(const ID &table_id,
                                                         uint32_t stable_page_id,
                                                         uint16_t stable_item_id,
                                                         const uint8_t *prior_tuple_data,
                                                         uint32_t prior_tuple_size)
    {
        if (savepoint_stack_.empty())
        {
            return;
        }

        Savepoint &savepoint = savepoint_stack_.back();
        if (findSavepointBackoutChange(savepoint, table_id, stable_page_id, stable_item_id) !=
            nullptr)
        {
            return;
        }

        SavepointBackoutAction action;
        action.table_id = table_id;
        action.stable_page_id = stable_page_id;
        action.stable_item_id = stable_item_id;
        if (prior_tuple_data != nullptr && prior_tuple_size > 0)
        {
            action.prior_tuple_image.assign(prior_tuple_data, prior_tuple_data + prior_tuple_size);
        }
        savepoint.changes.emplace_back(std::move(action));

        LOG_DEBUG(TRANSACTION,
                  "Tracked savepoint backout action (%s, page=%u, item=%u) in savepoint '%s' (level %u)",
                  prior_tuple_data != nullptr && prior_tuple_size > 0 ? "restore" : "purge",
                  stable_page_id,
                  stable_item_id,
                  savepoint.name.c_str(),
                  savepoint.level);
    }

    void ConnectionContext::mergeSavepointBackoutChanges(Savepoint &target,
                                                         const Savepoint &source)
    {
        for (const auto &change : source.changes)
        {
            if (findSavepointBackoutChange(target,
                                           change.table_id,
                                           change.stable_page_id,
                                           change.stable_item_id) != nullptr)
            {
                continue;
            }
            target.changes.emplace_back(change);
        }
    }

    auto ConnectionContext::collectRollbackBackoutChanges(size_t first_savepoint_index) const
        -> std::vector<SavepointBackoutAction>
    {
        std::vector<SavepointBackoutAction> changes;
        for (size_t idx = savepoint_stack_.size(); idx > first_savepoint_index; --idx)
        {
            const Savepoint &savepoint = savepoint_stack_[idx - 1];
            for (auto change_it = savepoint.changes.rbegin();
                 change_it != savepoint.changes.rend();
                 ++change_it)
            {
                const auto &change = *change_it;
                auto existing = std::find_if(changes.begin(),
                                             changes.end(),
                                             [&](const SavepointBackoutAction &candidate) {
                                                 return candidate.matches(change.table_id,
                                                                          change.stable_page_id,
                                                                          change.stable_item_id);
                                             });
                if (existing == changes.end())
                {
                    changes.emplace_back(change);
                }
            }
        }
        return changes;
    }

    Status ConnectionContext::rollbackToSavepoint(const std::string &name, ErrorContext *ctx)
    {
        if (current_xid_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Cannot rollback savepoint outside of a transaction");
            return Status::INVALID_ARGUMENT;
        }

        // Find the named savepoint
        auto sp_it = savepoint_stack_.end();
        for (auto it = savepoint_stack_.begin(); it != savepoint_stack_.end(); ++it)
        {
            if (it->name == name)
            {
                sp_it = it;
                break;
            }
        }

        if (sp_it == savepoint_stack_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Savepoint not found");
            return Status::NOT_FOUND;
        }

        LOG_DEBUG(TRANSACTION, "Rolling back to savepoint '%s' at level %u: proc_id=%u, xid=%lu",
                  name.c_str(), sp_it->level, proc_id_, current_xid_);

        MgaBackoutEngine *backout_engine = db_->mga_backout_engine();
        if (!backout_engine)
        {
            SET_ERROR_CONTEXT(ctx, Status::IO_ERROR, "MGA backout engine not available");
            return Status::IO_ERROR;
        }

        const size_t target_index = static_cast<size_t>(std::distance(savepoint_stack_.begin(),
                                                                      sp_it));

        struct RollbackGuard
        {
            bool &flag;
            explicit RollbackGuard(bool &target_flag) : flag(target_flag)
            {
                flag = true;
            }
            ~RollbackGuard()
            {
                flag = false;
            }
        } rollback_guard(savepoint_rollback_in_progress_);

        const auto rollback_changes = collectRollbackBackoutChanges(target_index);
        Status rollback_status =
            backout_engine->applySavepointBackout(rollback_changes, current_xid_, ctx);
        if (rollback_status != Status::OK)
        {
            return rollback_status;
        }

        // Keep the target savepoint active, but clear its post-rollback mutation state.
        sp_it = savepoint_stack_.begin() + static_cast<std::ptrdiff_t>(target_index);
        sp_it->changes.clear();

        // Remove nested savepoints.
        auto erase_from = sp_it;
        ++erase_from;
        savepoint_stack_.erase(erase_from, savepoint_stack_.end());

        // Update savepoint level
        savepoint_level_ = sp_it->level;

        // Restore command ID from savepoint
        command_id_ = sp_it->command_id;

        LOG_DEBUG(TRANSACTION,
                  "Rolled back to savepoint '%s': proc_id=%u, xid=%lu, new_level=%u", name.c_str(),
                  proc_id_, current_xid_, savepoint_level_);

        return Status::OK;
    }

    Status ConnectionContext::releaseSavepoint(const std::string &name, ErrorContext *ctx)
    {
        if (current_xid_ == 0)
        {
            SET_ERROR_CONTEXT(ctx, Status::INVALID_ARGUMENT,
                              "Cannot release savepoint outside of a transaction");
            return Status::INVALID_ARGUMENT;
        }

        // Find the named savepoint
        auto sp_it = savepoint_stack_.end();
        size_t sp_index = 0;
        for (auto it = savepoint_stack_.begin(); it != savepoint_stack_.end(); ++it, ++sp_index)
        {
            if (it->name == name)
            {
                sp_it = it;
                break;
            }
        }

        if (sp_it == savepoint_stack_.end())
        {
            SET_ERROR_CONTEXT(ctx, Status::NOT_FOUND, "Savepoint not found");
            return Status::NOT_FOUND;
        }

        LOG_DEBUG(TRANSACTION, "Releasing savepoint '%s' at level %u: proc_id=%u, xid=%lu",
                  name.c_str(), sp_it->level, proc_id_, current_xid_);

        // If there's a parent savepoint, merge canonical backout records for the released
        // savepoint and any nested descendants into it, preserving the earliest pre-change row
        // state as the authoritative undo image.
        if (sp_index > 0)
        {
            auto &parent = savepoint_stack_[sp_index - 1];
            for (size_t idx = sp_index; idx < savepoint_stack_.size(); ++idx)
            {
                mergeSavepointBackoutChanges(parent, savepoint_stack_[idx]);
            }
        }

        // Remove this savepoint and all nested ones
        savepoint_stack_.erase(sp_it, savepoint_stack_.end());

        // Update savepoint level
        if (!savepoint_stack_.empty())
        {
            savepoint_level_ = savepoint_stack_.back().level;
        }
        else
        {
            savepoint_level_ = 0;
        }

        LOG_DEBUG(TRANSACTION, "Released savepoint '%s': proc_id=%u, xid=%lu, new_level=%u",
                  name.c_str(), proc_id_, current_xid_, savepoint_level_);

        return Status::OK;
    }

    void ConnectionContext::trackTupleMutation(const ID& table_id,
                                               uint32_t page_id,
                                               uint16_t item_id,
                                               const uint8_t *prior_tuple_data,
                                               uint32_t prior_tuple_size)
    {
        if (savepoint_rollback_in_progress_ || savepoint_stack_.empty())
        {
            return;
        }
        recordSavepointBackoutAction(table_id,
                                     page_id,
                                     item_id,
                                     prior_tuple_data,
                                     prior_tuple_size);
    }

    void ConnectionContext::trackTupleMutation(uint32_t page_id,
                                               uint16_t item_id,
                                               const uint8_t *prior_tuple_data,
                                               uint32_t prior_tuple_size)
    {
        trackTupleMutation(ID{}, page_id, item_id, prior_tuple_data, prior_tuple_size);
    }

    // ============================================================================
    // Security Context Management (Phase 2 - Security System)
    // ============================================================================

    void ConnectionContext::setSessionContext(const ID& session_id, const ID& authkey_id,
                                             const std::string& emulation_mode,
                                             uint64_t policy_epoch_global,
                                             uint64_t policy_epoch_table)
    {
        const bool initial_binding = isZeroUuidLocal(session_id_) && isZeroUuidLocal(authkey_id_);
        if (initial_binding || current_xid_ == 0)
        {
            session_id_ = session_id;
            authkey_id_ = authkey_id;
            emulation_mode_ = emulation_mode.empty() ? "native" : emulation_mode;
            policy_epoch_global_ = policy_epoch_global;
            policy_epoch_table_ = policy_epoch_table;
            Status s = ProcArrayManager::setSessionId(proc_id_, session_id_, nullptr);
            if (s != Status::OK)
            {
                LOG_WARNING(TRANSACTION, "Failed to set session id in ProcArray for proc_id %u",
                            proc_id_);
            }
            LOG_DEBUG(TRANSACTION, "Set session context: proc_id=%u, session_id=%s, authkey_id=%s",
                      proc_id_, session_id.toString().c_str(), authkey_id.toString().c_str());
            refreshActiveTransactionAttribution();
            return;
        }

        pending_session_change_ = true;
        pending_session_id_ = session_id;
        pending_authkey_id_ = authkey_id;
        pending_emulation_mode_ = emulation_mode.empty() ? emulation_mode_ : emulation_mode;
        pending_policy_epoch_global_ = policy_epoch_global;
        pending_policy_epoch_table_ = policy_epoch_table;
        security_context_staged_ = true;

        LOG_DEBUG(TRANSACTION,
                  "Staged session context change for next transaction: proc_id=%u, session_id=%s",
                  proc_id_, session_id.toString().c_str());
    }

    ConnectionContext::ID ConnectionContext::effectiveSessionId() const
    {
        if (!isZeroUuidLocal(session_id_))
        {
            return session_id_;
        }
        if (!isZeroUuidLocal(protocol_session_id_))
        {
            return protocol_session_id_;
        }
        return attachment_id_;
    }

    void ConnectionContext::setSessionVariable(const std::string& name, const std::string& value)
    {
        std::lock_guard<std::mutex> lock(session_vars_mutex_);
        session_variables_[normalizeSessionVar(name)] = value;
    }

    bool ConnectionContext::getSessionVariable(const std::string& name, std::string& out) const
    {
        std::lock_guard<std::mutex> lock(session_vars_mutex_);
        auto it = session_variables_.find(normalizeSessionVar(name));
        if (it == session_variables_.end())
        {
            return false;
        }
        out = it->second;
        return true;
    }

    void ConnectionContext::clearSessionVariable(const std::string& name)
    {
        std::lock_guard<std::mutex> lock(session_vars_mutex_);
        session_variables_.erase(normalizeSessionVar(name));
    }

    void ConnectionContext::clearSessionVariables()
    {
        std::lock_guard<std::mutex> lock(session_vars_mutex_);
        session_variables_.clear();
    }

    std::vector<std::pair<std::string, std::string>> ConnectionContext::listSessionVariables() const
    {
        std::lock_guard<std::mutex> lock(session_vars_mutex_);
        std::vector<std::pair<std::string, std::string>> variables;
        variables.reserve(session_variables_.size());
        for (const auto& entry : session_variables_)
        {
            variables.emplace_back(entry.first, entry.second);
        }
        return variables;
    }

    void ConnectionContext::setCurrentUser(const ID& user_id, bool is_superuser)
    {
        // WP-5 EXEC-M3: Initialize session user on first call (authentication)
        // Session user is the original authenticated user and never changes
        static const ID zero_id{};
        const bool initial_binding = (session_user_id_ == zero_id);
        if (initial_binding)
        {
            session_user_id_ = user_id;
            session_is_superuser_ = is_superuser;
            LOG_DEBUG(TRANSACTION, "Set session user: proc_id=%u, session_user_id=%s, session_is_superuser=%d",
                      proc_id_, user_id.toString().c_str(), is_superuser);
        }

        if (current_xid_ != 0 && security_context_initialized_ && !initial_binding)
        {
            pending_user_change_ = true;
            pending_user_id_ = user_id;
            pending_is_superuser_ = is_superuser;
            pending_role_change_ = true;
            std::memset(&pending_role_id_, 0, sizeof(pending_role_id_));
            security_context_staged_ = true;

            LOG_DEBUG(TRANSACTION,
                      "Staged user change for next transaction: proc_id=%u, user_id=%s",
                      proc_id_, user_id.toString().c_str());
            return;
        }

        current_user_id_ = user_id;
        is_superuser_ = is_superuser;

        // Clear active role when user changes (switching users resets session state)
        std::memset(&active_role_id_, 0, sizeof(active_role_id_));
        security_context_initialized_ = true;

        LOG_DEBUG(TRANSACTION, "Set current user: proc_id=%u, user_id=%s, is_superuser=%d",
                  proc_id_, user_id.toString().c_str(), is_superuser);
        refreshActiveTransactionAttribution();
    }

    void ConnectionContext::setActiveRole(const ID& role_id)
    {
        if (current_xid_ != 0 && security_context_initialized_)
        {
            pending_role_change_ = true;
            pending_role_id_ = role_id;
            security_context_staged_ = true;

            LOG_DEBUG(TRANSACTION,
                      "Staged role change for next transaction: proc_id=%u, role_id=%s",
                      proc_id_, role_id.toString().c_str());
            return;
        }

        active_role_id_ = role_id;

        LOG_DEBUG(TRANSACTION, "Set active role: proc_id=%u, role_id=%s",
                  proc_id_, role_id.toString().c_str());
        refreshActiveTransactionAttribution();
    }

    void ConnectionContext::clearActiveRole()
    {
        if (current_xid_ != 0 && security_context_initialized_)
        {
            pending_role_change_ = true;
            std::memset(&pending_role_id_, 0, sizeof(pending_role_id_));
            security_context_staged_ = true;

            LOG_DEBUG(TRANSACTION, "Staged role clear for next transaction: proc_id=%u",
                      proc_id_);
            return;
        }

        LOG_DEBUG(TRANSACTION, "Clearing active role: proc_id=%u, previous_role=%s",
                  proc_id_, active_role_id_.toString().c_str());

        std::memset(&active_role_id_, 0, sizeof(active_role_id_));
        refreshActiveTransactionAttribution();
    }

    void ConnectionContext::setRoleSwitchPolicy(RoleSwitchPolicy policy)
    {
        role_switch_policy_ = policy;
    }

    // ============================================================================
    // Security Context Stack (Phase 3.1 - SQL Object Permissions)
    // ============================================================================

    void ConnectionContext::pushSecurityContext(const ID& user_id, const ID& role_id,
                                               bool is_superuser_flag, SecurityMode mode,
                                               const ID& object_id)
    {
        SecurityContext ctx;
        ctx.effective_user_id = user_id;
        ctx.effective_role_id = role_id;
        ctx.is_superuser = is_superuser_flag;
        ctx.mode = mode;
        ctx.object_id = object_id;
        ctx.session_id = session_id_;
        ctx.authkey_id = authkey_id_;
        ctx.emulation_mode = emulation_mode_;
        ctx.policy_epoch_global = policy_epoch_global_;
        ctx.policy_epoch_table = policy_epoch_table_;

        security_stack_.push_back(ctx);

        LOG_DEBUG(TRANSACTION, "Pushed security context: proc_id=%u, depth=%zu, mode=%s, user=%s",
                  proc_id_, security_stack_.size(),
                  mode == SecurityMode::DEFINER ? "DEFINER" : "INVOKER",
                  user_id.toString().c_str());
    }

    void ConnectionContext::popSecurityContext()
    {
        if (!security_stack_.empty())
        {
            security_stack_.pop_back();

            LOG_DEBUG(TRANSACTION, "Popped security context: proc_id=%u, depth=%zu",
                      proc_id_, security_stack_.size());
        }
        else
        {
            LOG_WARNING(TRANSACTION, "Attempted to pop empty security context stack: proc_id=%u",
                       proc_id_);
        }
    }

    ConnectionContext::SecurityContext ConnectionContext::getCurrentSecurityContext() const
    {
        if (!security_stack_.empty())
        {
            // Return the top of the stack (most recent context)
            return security_stack_.back();
        }

        // No stacked context - return base connection context
        SecurityContext ctx;
        ctx.effective_user_id = current_user_id_;
        ctx.effective_role_id = active_role_id_;
        ctx.is_superuser = is_superuser_;
        ctx.mode = SecurityMode::INVOKER;  // Base context is always INVOKER
        std::memset(&ctx.object_id, 0, sizeof(ctx.object_id));  // No object
        ctx.session_id = session_id_;
        ctx.authkey_id = authkey_id_;
        ctx.emulation_mode = emulation_mode_;
        ctx.policy_epoch_global = policy_epoch_global_;
        ctx.policy_epoch_table = policy_epoch_table_;

        return ctx;
    }

    std::vector<ConnectionContext::SecurityContext> ConnectionContext::listSecurityContextStack() const
    {
        return security_stack_;
    }

    bool ConnectionContext::isDefinerContext() const
    {
        if (!security_stack_.empty())
        {
            return security_stack_.back().mode == SecurityMode::DEFINER;
        }
        return false;  // Base context is always INVOKER
    }

    // P2-7: Deferred Constraint Support Implementation

    bool ConnectionContext::isConstraintDeferred(const ID& constraint_id) const
    {
        // Check global deferral first
        if (all_constraints_deferred_)
        {
            return true;
        }

        // Check per-constraint state
        auto it = constraint_deferred_state_.find(constraint_id);
        if (it != constraint_deferred_state_.end())
        {
            return it->second;
        }

        // Default: not deferred (INITIALLY IMMEDIATE)
        return false;
    }

    void ConnectionContext::setConstraintDeferred(const ID& constraint_id, bool deferred)
    {
        constraint_deferred_state_[constraint_id] = deferred;

        // If setting to IMMEDIATE, validate any pending checks for this constraint
        if (!deferred)
        {
            // Validate pending checks for this specific constraint
            std::vector<DeferredConstraintCheck> remaining_checks;
            for (const auto& check : deferred_checks_)
            {
                if (check.constraint_id == constraint_id)
                {
                    // Would validate here - constraint validation would happen
                    (void)check; // Suppress unused warning
                }
                else
                {
                    remaining_checks.push_back(check);
                }
            }
            deferred_checks_ = std::move(remaining_checks);
        }
    }

    void ConnectionContext::setAllConstraintsDeferred(bool deferred)
    {
        all_constraints_deferred_ = deferred;

        // If setting to IMMEDIATE, validate all pending checks
        if (!deferred)
        {
            // Validate all pending checks - in full implementation would re-check constraints
            (void)deferred_checks_; // All checks would be validated
            deferred_checks_.clear();
        }
    }

    void ConnectionContext::addDeferredConstraintCheck(const DeferredConstraintCheck& check)
    {
        deferred_checks_.push_back(check);
    }

    Status ConnectionContext::validateDeferredConstraints(ErrorContext* ctx)
    {
        (void)ctx; // Will be used in full implementation

        // Called at COMMIT time to validate all deferred constraints
        // In a full implementation, we would:
        // 1. Look up the constraint from catalog
        // 2. Re-execute the constraint check with stored values
        // 3. Return error if any check fails

        // Placeholder: For now, all deferred constraints pass
        // Full implementation would call catalog_manager methods to re-validate

        // Clear validated checks
        deferred_checks_.clear();

        return Status::OK;
    }

    void ConnectionContext::clearDeferredConstraints()
    {
        deferred_checks_.clear();
        constraint_deferred_state_.clear();
        all_constraints_deferred_ = false;
    }

    void ConnectionContext::initializeConstraintStates()
    {
        // Clear any existing state
        constraint_deferred_state_.clear();
        all_constraints_deferred_ = false;

        // In a full implementation, we would query the catalog for all
        // DEFERRABLE constraints and set their initial state based on
        // INITIALLY DEFERRED / INITIALLY IMMEDIATE

        // For now, all constraints start as IMMEDIATE (not deferred)
    }

    // =========================================================================
    // P2-21: Prepared Statement Cache Implementation
    // =========================================================================

    Status ConnectionContext::prepareStatement(const std::string& name,
                                               const std::string& sql_text,
                                               const std::vector<uint8_t>& bytecode,
                                               const std::vector<uint16_t>& param_types,
                                               ErrorContext* ctx)
    {
        auto now_micros = []() -> int64_t {
            return static_cast<int64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
        };

        // Check if name already exists
        if (prepared_statements_.find(name) != prepared_statements_.end())
        {
            if (ctx)
            {
                ctx->code = Status::CONSTRAINT_VIOLATION;
                ctx->message = "Prepared statement '" + name + "' already exists";
            }
            return Status::CONSTRAINT_VIOLATION;
        }

        // Check if we need to evict (LRU) before adding
        if (max_prepared_statements_ > 0 &&
            prepared_statements_.size() >= max_prepared_statements_)
        {
            evictOldestPreparedStatement();
        }

        // Create the prepared statement
        PreparedStatement stmt;
        stmt.name = name;
        stmt.sql_text = sql_text;
        stmt.bytecode = bytecode;
        stmt.param_types = param_types;
        stmt.param_count = param_types.size();
        stmt.created_at = std::chrono::steady_clock::now();
        stmt.last_used = stmt.created_at;
        stmt.created_at_micros = now_micros();
        stmt.last_used_micros = stmt.created_at_micros;
        stmt.execution_count = 0;

        prepared_statements_[name] = std::move(stmt);
        return Status::OK;
    }

    ConnectionContext::PreparedStatement* ConnectionContext::getPreparedStatement(const std::string& name)
    {
        auto it = prepared_statements_.find(name);
        if (it != prepared_statements_.end())
        {
            return &it->second;
        }
        return nullptr;
    }

    Status ConnectionContext::deallocatePreparedStatement(const std::string& name, ErrorContext* ctx)
    {
        if (name.empty())
        {
            // DEALLOCATE ALL
            prepared_statements_.clear();
            return Status::OK;
        }

        auto it = prepared_statements_.find(name);
        if (it == prepared_statements_.end())
        {
            if (ctx)
            {
                ctx->code = Status::NOT_FOUND;
                ctx->message = "Prepared statement '" + name + "' does not exist";
            }
            return Status::NOT_FOUND;
        }

        prepared_statements_.erase(it);
        return Status::OK;
    }

    void ConnectionContext::recordStatementExecution(const std::string& name)
    {
        auto it = prepared_statements_.find(name);
        if (it != prepared_statements_.end())
        {
            it->second.last_used = std::chrono::steady_clock::now();
            it->second.execution_count++;
            it->second.last_used_micros = static_cast<int64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count());
        }
    }

    void ConnectionContext::getPreparedStatementStats(size_t& count, size_t& total_bytes) const
    {
        count = prepared_statements_.size();
        total_bytes = 0;

        for (const auto& [name, stmt] : prepared_statements_)
        {
            total_bytes += name.size();
            total_bytes += stmt.sql_text.size();
            total_bytes += stmt.bytecode.size();
            total_bytes += stmt.param_types.size() * sizeof(uint16_t);
            total_bytes += sizeof(PreparedStatement);  // Struct overhead
        }
    }

    void ConnectionContext::listPreparedStatements(std::vector<PreparedStatementInfo>& out) const
    {
        out.clear();
        out.reserve(prepared_statements_.size());

        for (const auto& [name, stmt] : prepared_statements_)
        {
            PreparedStatementInfo info;
            info.name = name;
            info.sql_text = stmt.sql_text;
            info.param_count = stmt.param_count;
            info.execution_count = stmt.execution_count;
            info.created_at_micros = stmt.created_at_micros;
            info.last_used_micros = stmt.last_used_micros;
            info.memory_bytes = name.size() + stmt.sql_text.size() +
                stmt.bytecode.size() +
                stmt.param_types.size() * sizeof(uint16_t) +
                sizeof(PreparedStatement);
            out.push_back(std::move(info));
        }
    }

    void ConnectionContext::evictOldestPreparedStatement()
    {
        if (prepared_statements_.empty())
        {
            return;
        }

        // Find the least recently used statement
        auto oldest_it = prepared_statements_.begin();
        auto oldest_time = oldest_it->second.last_used;

        for (auto it = prepared_statements_.begin(); it != prepared_statements_.end(); ++it)
        {
            if (it->second.last_used < oldest_time)
            {
                oldest_time = it->second.last_used;
                oldest_it = it;
            }
        }

        // Evict it
        prepared_statements_.erase(oldest_it);

        auto& metrics = core::ScratchBirdMetrics::getInstance();
        metrics.initialize();
        if (metrics.statement_cache_evictions_total) {
            metrics.statement_cache_evictions_total->inc(1.0);
        }
    }

} // namespace scratchbird::core
