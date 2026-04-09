#include "scratchbird/sblr/native_sql_renderer.h"

#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "scratchbird/sblr/native_sql_render_contract.h"
#include "scratchbird/sblr/v3_opcode_registry.h"

namespace scratchbird::sblr {

namespace {

using v3::Opcode;

constexpr std::string_view kAlterUserPrefix = "security.user.alter.";
constexpr std::string_view kConnRuleCreatePrefix = "security.connection_rule.create.";
constexpr std::string_view kConnRuleAlterPrefix = "security.connection_rule.alter.";
constexpr std::string_view kConnRuleDropPrefix = "security.connection_rule.drop.";
constexpr std::string_view kTokenCreatePrefix = "security.token.create.";
constexpr std::string_view kTokenAlterPrefix = "security.token.alter.";
constexpr std::string_view kTokenRevokePrefix = "security.token.revoke.";
constexpr std::string_view kTokenDropPrefix = "security.token.drop.";
constexpr std::string_view kQuotaCreatePrefix = "security.quota_profile.create.";
constexpr std::string_view kQuotaAlterPrefix = "security.quota_profile.alter.";
constexpr std::string_view kQuotaDropPrefix = "security.quota_profile.drop.";
constexpr std::string_view kMeasurementRetentionPrefix = "measurement.retention.";
constexpr size_t kUuidTextLength = 36;

bool startsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

const v3::Value::Object* objectPayload(const v3::Instruction& instruction) {
    return std::get_if<v3::Value::Object>(&instruction.payload.data);
}

const v3::Value* objectField(const v3::Value::Object& object, std::string_view key) {
    auto it = object.find(std::string(key));
    if (it == object.end()) {
        return nullptr;
    }
    return &it->second;
}

std::optional<uint64_t> asU64(const v3::Value& value) {
    if (const auto* u64 = std::get_if<uint64_t>(&value.data)) {
        return *u64;
    }
    if (const auto* i64 = std::get_if<int64_t>(&value.data)) {
        if (*i64 >= 0) {
            return static_cast<uint64_t>(*i64);
        }
    }
    return std::nullopt;
}

std::optional<bool> asBool(const v3::Value& value) {
    if (const auto* b = std::get_if<bool>(&value.data)) {
        return *b;
    }
    if (const auto* u64 = std::get_if<uint64_t>(&value.data)) {
        return *u64 != 0;
    }
    if (const auto* i64 = std::get_if<int64_t>(&value.data)) {
        return *i64 != 0;
    }
    return std::nullopt;
}

std::optional<std::string> asString(const v3::Value& value) {
    if (const auto* s = std::get_if<std::string>(&value.data)) {
        return *s;
    }
    if (const auto* bytes = std::get_if<v3::Value::Bytes>(&value.data)) {
        return std::string(bytes->begin(), bytes->end());
    }
    return std::nullopt;
}

bool isAsciiHex(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool looksLikeUuidText(std::string_view text) {
    if (text.size() != kUuidTextLength) {
        return false;
    }
    for (size_t i = 0; i < text.size(); ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (text[i] != '-') {
                return false;
            }
            continue;
        }
        if (!isAsciiHex(text[i])) {
            return false;
        }
    }
    return true;
}

std::string resolveNameToken(std::string token,
                             NativeSqlNameResolver* resolver,
                             NativeSqlObjectTypeHint hint) {
    if (resolver == nullptr || token.empty() || !looksLikeUuidText(token)) {
        return token;
    }
    std::string resolved;
    if (resolver->resolveNameByUuid(token, hint, resolved) && !resolved.empty()) {
        return resolved;
    }
    return token;
}

std::string sqlQuote(std::string_view text) {
    std::string out;
    out.reserve(text.size() + 2);
    out.push_back('\'');
    for (char c : text) {
        if (c == '\'') {
            out.push_back('\'');
        }
        out.push_back(c);
    }
    out.push_back('\'');
    return out;
}

std::string renderSchemaPath(const v3::Value& value,
                             NativeSqlNameResolver* resolver,
                             NativeSqlObjectTypeHint hint) {
    const auto* list = std::get_if<v3::Value::List>(&value.data);
    if (list == nullptr || list->empty()) {
        return {};
    }
    std::string out;
    for (size_t i = 0; i < list->size(); ++i) {
        if (const auto part = asString((*list)[i])) {
            if (!out.empty()) {
                out.push_back('.');
            }
            out.append(resolveNameToken(*part, resolver, hint));
        }
    }
    return out;
}

std::string renderU64List(const v3::Value& value) {
    const auto* list = std::get_if<v3::Value::List>(&value.data);
    if (list == nullptr || list->empty()) {
        return {};
    }
    std::string out;
    for (size_t i = 0; i < list->size(); ++i) {
        const auto item = asU64((*list)[i]);
        if (!item.has_value()) {
            continue;
        }
        if (!out.empty()) {
            out.append(", ");
        }
        out.append(std::to_string(*item));
    }
    return out;
}

std::string compareOpName(uint64_t op) {
    switch (op) {
        case 0: return "EQ";
        case 1: return "NE";
        case 2: return "LT";
        case 3: return "LE";
        case 4: return "GT";
        case 5: return "GE";
        case 6: return "EXISTS";
        case 7: return "NOT_EXISTS";
        default: return "EQ";
    }
}

std::string scorerName(uint64_t scorer) {
    switch (scorer) {
        case 1: return "BM25";
        case 2: return "TFIDF";
        case 3: return "DFR";
        default: return "BM25";
    }
}

std::string metricName(uint64_t metric) {
    switch (metric) {
        case 1: return "L2";
        case 2: return "COSINE";
        case 3: return "DOT";
        default: return "L2";
    }
}

std::string bridgeModeName(uint64_t mode) {
    switch (mode) {
        case 1: return "HASH_SHUFFLE";
        case 2: return "RANGE_SHUFFLE";
        case 3: return "BROADCAST";
        default: return "HASH_SHUFFLE";
    }
}

std::string suffixAfterPrefix(const std::string& full_key, std::string_view prefix) {
    if (!startsWith(full_key, prefix)) {
        return {};
    }
    return full_key.substr(prefix.size());
}

std::optional<std::string> literalStringFromInstrPtr(const v3::Value& value) {
    const auto* instr_ptr = std::get_if<v3::Value::InstrPtr>(&value.data);
    if (instr_ptr == nullptr || !(*instr_ptr)) {
        return std::nullopt;
    }
    const auto* payload = objectPayload(*(*instr_ptr));
    if (payload == nullptr) {
        return std::nullopt;
    }
    const auto* literal = objectField(*payload, "value");
    if (literal == nullptr) {
        return std::nullopt;
    }
    return asString(*literal);
}

std::string renderExprLikeValue(const v3::Value& value) {
    if (const auto literal = literalStringFromInstrPtr(value)) {
        return sqlQuote(*literal);
    }
    if (const auto u64 = asU64(value)) {
        return std::to_string(*u64);
    }
    if (const auto s = asString(value)) {
        return sqlQuote(*s);
    }
    if (const auto b = asBool(value)) {
        return *b ? "TRUE" : "FALSE";
    }

    const auto* instr_ptr = std::get_if<v3::Value::InstrPtr>(&value.data);
    if (instr_ptr != nullptr && (*instr_ptr)) {
        const v3::Instruction& literal_instr = *(*instr_ptr);
        if (literal_instr.opcode == static_cast<uint16_t>(Opcode::SBLR3_LITERAL_NULL)) {
            return "NULL";
        }
        const auto* payload = objectPayload(literal_instr);
        if (payload != nullptr) {
            if (const auto* literal = objectField(*payload, "value")) {
                if (const auto s = asString(*literal)) {
                    return sqlQuote(*s);
                }
                if (const auto u64 = asU64(*literal)) {
                    return std::to_string(*u64);
                }
                if (const auto b = asBool(*literal)) {
                    return *b ? "TRUE" : "FALSE";
                }
            }
        }
    }

    return "<expr>";
}

std::string renderAlterSystemClassifierStatement(const std::string& contract_id,
                                                 const v3::Value::Object& payload,
                                                 NativeSqlNameResolver* resolver) {
    const auto action_value = objectField(payload, "action");
    const auto key_value = objectField(payload, "key");
    const auto target_value = objectField(payload, "target");
    const auto value_value = objectField(payload, "value");
    const uint64_t action = action_value != nullptr ? asU64(*action_value).value_or(1) : 1;
    const std::string key = key_value != nullptr && asString(*key_value).has_value()
                                ? *asString(*key_value)
                                : std::string();
    const std::string target = target_value != nullptr && asString(*target_value).has_value()
                                   ? *asString(*target_value)
                                   : std::string();
    const std::string value_sql = value_value != nullptr
                                      ? renderExprLikeValue(*value_value)
                                      : std::string();
    if (contract_id == "NRSQL-012-ALTER-USER") {
        const std::string user_name = resolveNameToken(
            suffixAfterPrefix(key, kAlterUserPrefix), resolver, NativeSqlObjectTypeHint::USER);
        std::vector<std::string> options;
        if (value_value != nullptr) {
            const std::string raw = literalStringFromInstrPtr(*value_value).value_or("");
            size_t start = 0;
            while (start <= raw.size()) {
                size_t end = raw.find(';', start);
                if (end == std::string::npos) {
                    end = raw.size();
                }
                std::string token = raw.substr(start, end - start);
                if (!token.empty()) {
                    if (startsWith(token, "PASSWORD=")) {
                        options.push_back("PASSWORD " + sqlQuote(token.substr(9)));
                    } else if (token == "SUPERUSER=1") {
                        options.push_back("SUPERUSER");
                    } else if (token == "SUPERUSER=0") {
                        options.push_back("NOSUPERUSER");
                    } else {
                        options.push_back(token);
                    }
                }
                if (end == raw.size()) {
                    break;
                }
                start = end + 1;
            }
        }
        std::ostringstream sql;
        sql << "ALTER USER " << user_name;
        if (!options.empty()) {
            sql << " WITH";
            for (const auto& opt : options) {
                sql << " " << opt;
            }
        }
        return sql.str();
    }

    if (contract_id == "NRSQL-020-CONNECTION-RULE-CREATE") {
        const std::string name = resolveNameToken(
            suffixAfterPrefix(key, kConnRuleCreatePrefix), resolver, NativeSqlObjectTypeHint::UNKNOWN);
        const std::string rule_payload = value_value != nullptr
                                             ? literalStringFromInstrPtr(*value_value).value_or("")
                                             : std::string();
        return rule_payload.empty() ? "CREATE CONNECTION RULE " + name
                                    : "CREATE CONNECTION RULE " + name + " " + rule_payload;
    }

    if (contract_id == "NRSQL-021-CONNECTION-RULE-ALTER") {
        const std::string name = resolveNameToken(
            suffixAfterPrefix(key, kConnRuleAlterPrefix), resolver, NativeSqlObjectTypeHint::UNKNOWN);
        const std::string rule_payload = value_value != nullptr
                                             ? literalStringFromInstrPtr(*value_value).value_or("")
                                             : std::string();
        return rule_payload.empty() ? "ALTER CONNECTION RULE " + name
                                    : "ALTER CONNECTION RULE " + name + " SET (" + rule_payload + ")";
    }

    if (contract_id == "NRSQL-022-CONNECTION-RULE-DROP") {
        return "DROP CONNECTION RULE " +
               resolveNameToken(
                   suffixAfterPrefix(key, kConnRuleDropPrefix),
                   resolver,
                   NativeSqlObjectTypeHint::UNKNOWN);
    }

    if (contract_id == "NRSQL-023-TOKEN-CREATE") {
        const std::string name = resolveNameToken(
            suffixAfterPrefix(key, kTokenCreatePrefix), resolver, NativeSqlObjectTypeHint::UNKNOWN);
        const std::string token_payload = value_value != nullptr
                                              ? literalStringFromInstrPtr(*value_value).value_or("")
                                              : std::string();
        return token_payload.empty() ? "CREATE TOKEN " + name
                                     : "CREATE TOKEN " + name + " WITH " + token_payload;
    }

    if (contract_id == "NRSQL-024-TOKEN-ALTER") {
        const std::string name = resolveNameToken(
            suffixAfterPrefix(key, kTokenAlterPrefix), resolver, NativeSqlObjectTypeHint::UNKNOWN);
        const std::string token_payload = value_value != nullptr
                                              ? literalStringFromInstrPtr(*value_value).value_or("")
                                              : std::string();
        return token_payload.empty() ? "ALTER TOKEN " + name
                                     : "ALTER TOKEN " + name + " SET (" + token_payload + ")";
    }

    if (contract_id == "NRSQL-025-TOKEN-REVOKE") {
        return "REVOKE TOKEN " +
               resolveNameToken(
                   suffixAfterPrefix(key, kTokenRevokePrefix),
                   resolver,
                   NativeSqlObjectTypeHint::UNKNOWN);
    }

    if (contract_id == "NRSQL-026-TOKEN-DROP") {
        return "DROP TOKEN " +
               resolveNameToken(
                   suffixAfterPrefix(key, kTokenDropPrefix),
                   resolver,
                   NativeSqlObjectTypeHint::UNKNOWN);
    }

    if (contract_id == "NRSQL-027-QUOTA-PROFILE-CREATE") {
        const std::string name = resolveNameToken(
            suffixAfterPrefix(key, kQuotaCreatePrefix), resolver, NativeSqlObjectTypeHint::UNKNOWN);
        const std::string payload_text = value_value != nullptr
                                             ? literalStringFromInstrPtr(*value_value).value_or("")
                                             : std::string();
        return payload_text.empty() ? "CREATE QUOTA PROFILE " + name
                                    : "CREATE QUOTA PROFILE " + name + " (" + payload_text + ")";
    }

    if (contract_id == "NRSQL-028-QUOTA-PROFILE-ALTER") {
        const std::string name = resolveNameToken(
            suffixAfterPrefix(key, kQuotaAlterPrefix), resolver, NativeSqlObjectTypeHint::UNKNOWN);
        const std::string payload_text = value_value != nullptr
                                             ? literalStringFromInstrPtr(*value_value).value_or("")
                                             : std::string();
        return payload_text.empty() ? "ALTER QUOTA PROFILE " + name
                                    : "ALTER QUOTA PROFILE " + name + " SET (" + payload_text + ")";
    }

    if (contract_id == "NRSQL-029-QUOTA-PROFILE-DROP") {
        return "DROP QUOTA PROFILE " +
               resolveNameToken(
                   suffixAfterPrefix(key, kQuotaDropPrefix),
                   resolver,
                   NativeSqlObjectTypeHint::UNKNOWN);
    }

    if (contract_id == "NRSQL-030-MEASUREMENT-RETENTION") {
        const std::string name = resolveNameToken(
            suffixAfterPrefix(key, kMeasurementRetentionPrefix),
            resolver,
            NativeSqlObjectTypeHint::TABLE);
        if (value_sql.empty()) {
            return "ALTER MEASUREMENT " + name + " RETENTION";
        }
        return "ALTER MEASUREMENT " + name + " RETENTION " + value_sql;
    }

    if (action == 3) {
        return "CONFIG HISTORY";
    }
    if (action == 4) {
        return "CONFIG RELOAD";
    }
    if (key == "management.show_servers") {
        return "SHOW MANAGEMENT SERVERS";
    }
    if (key == "management.show_instructions") {
        return "SHOW MANAGEMENT INSTRUCTIONS";
    }
    if (key == "management.show_drift") {
        return "SHOW MANAGEMENT DRIFT";
    }
    if (action == 5) {
        std::ostringstream sql;
        sql << "ALTER SYSTEM ASSESS REMOTE SET " << key;
        if (!value_sql.empty()) {
            sql << " = " << value_sql;
        }
        if (!target.empty()) {
            sql << " ON SERVER " << target;
        }
        return sql.str();
    }
    if (action == 6) {
        return "ALTER SYSTEM APPLY INSTRUCTION " + key;
    }
    if (action == 7) {
        return "ALTER SYSTEM CANCEL INSTRUCTION " + key;
    }
    if (action == 8) {
        return "ALTER SYSTEM QUARANTINE INSTRUCTION " + key;
    }
    if (action == 9) {
        return "ALTER SYSTEM ACKNOWLEDGE INSTRUCTION " + key;
    }
    if (key.empty()) {
        return "ALTER SYSTEM";
    }
    if (action == 2) {
        return "ALTER SYSTEM RESET " + key;
    }
    if (value_sql.empty()) {
        return "ALTER SYSTEM SET " + key;
    }
    return "ALTER SYSTEM SET " + key + " = " + value_sql;
}

}  // namespace

bool renderNativeSqlInstruction(const v3::Instruction& instruction,
                                NativeSqlNameResolver* resolver,
                                NativeSqlRenderResult& out,
                                std::string& error) {
    out = {};
    error.clear();

    const NativeSqlRenderContract* contract = nativeSqlRenderContractForInstruction(instruction);
    if (contract == nullptr) {
        error = "No native SQL render contract found for opcode";
        return false;
    }

    out.contract_id = contract->contract_id;
    out.canonical_opcode_symbol = contract->canonical_opcode_symbol;
    out.result_shape = contract->result_shape;

    const auto* payload = objectPayload(instruction);

    auto fieldString = [&](std::string_view name) -> std::string {
        if (payload == nullptr) {
            return {};
        }
        const auto* v = objectField(*payload, name);
        if (v == nullptr) {
            return {};
        }
        const auto s = asString(*v);
        return s.has_value() ? *s : std::string();
    };

    auto fieldStringResolved = [&](std::string_view name,
                                   NativeSqlObjectTypeHint hint) -> std::string {
        return resolveNameToken(fieldString(name), resolver, hint);
    };

    auto fieldStringAny = [&](std::initializer_list<std::string_view> names) -> std::string {
        for (const auto name : names) {
            const std::string value = fieldString(name);
            if (!value.empty()) {
                return value;
            }
        }
        return {};
    };

    auto fieldU64 = [&](std::string_view name, uint64_t fallback = 0) -> uint64_t {
        if (payload == nullptr) {
            return fallback;
        }
        const auto* v = objectField(*payload, name);
        if (v == nullptr) {
            return fallback;
        }
        const auto u = asU64(*v);
        return u.has_value() ? *u : fallback;
    };

    auto fieldU64Any = [&](std::initializer_list<std::string_view> names,
                           uint64_t fallback = 0) -> uint64_t {
        for (const auto name : names) {
            if (payload == nullptr) {
                return fallback;
            }
            const auto* v = objectField(*payload, name);
            if (v == nullptr) {
                continue;
            }
            if (const auto u = asU64(*v)) {
                return *u;
            }
            if (const auto b = asBool(*v)) {
                return *b ? 1u : 0u;
            }
        }
        return fallback;
    };

    auto fieldBoolAny = [&](std::initializer_list<std::string_view> names,
                            bool fallback = false) -> bool {
        for (const auto name : names) {
            if (payload == nullptr) {
                return fallback;
            }
            const auto* v = objectField(*payload, name);
            if (v == nullptr) {
                continue;
            }
            if (const auto b = asBool(*v)) {
                return *b;
            }
            if (const auto u = asU64(*v)) {
                return *u != 0;
            }
        }
        return fallback;
    };

    const std::string contract_id = out.contract_id;

    if (contract_id == "NRSQL-001-DOC-PATH-FILTER" && payload != nullptr) {
        out.sql = "DOC PATH FILTER PATH_ID " +
                  std::to_string(fieldU64Any({"path_expr", "path_id"})) +
                  " OP " + compareOpName(fieldU64Any({"operator", "cmp"})) +
                  " VALUE_REF " + std::to_string(fieldU64Any({"value_expr", "value_ref"}));
        return true;
    }

    if (contract_id == "NRSQL-002-TS-BUCKET-AGG" && payload != nullptr) {
        std::string refs;
        if (const auto* agg_list = objectField(*payload, "agg_list")) {
            refs = renderU64List(*agg_list);
        } else if (const auto* agg_count = objectField(*payload, "agg_count")) {
            const uint64_t count = asU64(*agg_count).value_or(0);
            refs = std::string("<") + std::to_string(count) + " refs>";
        }
        out.sql = "TS BUCKET AGG TIME_EXPR " + std::to_string(fieldU64("time_expr")) +
                  " BUCKET_NS " + std::to_string(fieldU64Any({"bucket_size", "bucket_ns"})) +
                  " AGG_REFS (" + refs + ")";
        return true;
    }

    if (contract_id == "NRSQL-003-SEARCH-QUERY-DSL" && payload != nullptr) {
        const std::string payload_text =
            fieldStringAny({"dsl_payload_json", "payload", "dsl_blob_ref"});
        out.sql = "SEARCH QUERY DSL TARGET_INDEX " +
                  std::to_string(fieldU64Any({"target_index", "index_id"})) +
                  " PAYLOAD " + sqlQuote(payload_text) +
                  " SCORER " + scorerName(fieldU64("scorer_id", 1));
        return true;
    }

    if (contract_id == "NRSQL-004-VECTOR-ANN-QUERY" && payload != nullptr) {
        out.sql = "VECTOR ANN QUERY INDEX " + std::to_string(fieldU64Any({"vector_expr", "index_id"})) +
                  " METRIC " + metricName(fieldU64("metric", 1)) +
                  " TOPK " + std::to_string(fieldU64Any({"k", "topk"})) +
                  " EF_SEARCH " + std::to_string(fieldU64Any({"ef_search", "ef"}));
        return true;
    }

    if (contract_id == "NRSQL-005-HYBRID-BRIDGE" && payload != nullptr) {
        out.sql = "HYBRID BRIDGE EXCHANGE SOURCE_TRACK " +
                  std::to_string(fieldU64Any({"source_track", "src_track"})) +
                  " TARGET_TRACK " + std::to_string(fieldU64Any({"target_track", "dst_track"})) +
                  " MODE " + bridgeModeName(fieldU64Any({"bridge_mode", "mode"}, 1));
        return true;
    }

    if (contract_id == "NRSQL-006-UDR-COMPILE" && payload != nullptr) {
        const bool validate_only = fieldBoolAny({"validate_only"}, false);
        out.sql = std::string(validate_only ? "UDR VALIDATE" : "UDR COMPILE") +
                  " EMBEDDED PAYLOAD PROFILE " + fieldString("profile_id") +
                  " FORMAT " + fieldString("payload_format") +
                  " BYTES " + fieldString("payload_bytes") +
                  " SESSION_SIGNATURE " + fieldString("session_signature");
        return true;
    }

    if (contract_id == "NRSQL-007-UDR-TEMPLATE" && payload != nullptr) {
        const bool validate_only = fieldBoolAny({"validate_only"}, false);
        out.sql = std::string(validate_only ? "UDR VALIDATE" : "UDR COMPILE") +
                  " SQL TEMPLATE TEMPLATE_ID " + fieldString("template_id") +
                  " SQL_TEXT " + sqlQuote(fieldString("sql_text")) +
                  " PROFILE " + fieldString("profile_id") +
                  " SESSION_SIGNATURE " + fieldString("session_signature");
        return true;
    }

    if (contract_id == "NRSQL-010-CREATE-DATABASE-EMULATED" && payload != nullptr) {
        out.sql = "CREATE DATABASE " + fieldStringResolved("name", NativeSqlObjectTypeHint::DATABASE);
        return true;
    }

    if (contract_id == "NRSQL-011-CREATE-USER" && payload != nullptr) {
        out.sql = "CREATE USER " + fieldStringResolved("name", NativeSqlObjectTypeHint::USER);
        return true;
    }

    if ((contract_id == "NRSQL-012-ALTER-USER" ||
         contract_id == "NRSQL-020-CONNECTION-RULE-CREATE" ||
         contract_id == "NRSQL-021-CONNECTION-RULE-ALTER" ||
         contract_id == "NRSQL-022-CONNECTION-RULE-DROP" ||
         contract_id == "NRSQL-023-TOKEN-CREATE" ||
         contract_id == "NRSQL-024-TOKEN-ALTER" ||
         contract_id == "NRSQL-025-TOKEN-REVOKE" ||
         contract_id == "NRSQL-026-TOKEN-DROP" ||
         contract_id == "NRSQL-027-QUOTA-PROFILE-CREATE" ||
         contract_id == "NRSQL-028-QUOTA-PROFILE-ALTER" ||
         contract_id == "NRSQL-029-QUOTA-PROFILE-DROP" ||
        contract_id == "NRSQL-030-MEASUREMENT-RETENTION" ||
         contract_id == "NRSQL-100-ALTER-SYSTEM-GENERIC") &&
        payload != nullptr) {
        out.sql = renderAlterSystemClassifierStatement(contract_id, *payload, resolver);
        return true;
    }

    if (contract_id == "NRSQL-013-DROP-USER" && payload != nullptr) {
        std::string name;
        if (const auto* path = objectField(*payload, "path")) {
            name = renderSchemaPath(*path, resolver, NativeSqlObjectTypeHint::USER);
        }
        out.sql = "DROP USER " + name;
        return true;
    }

    if ((contract_id == "NRSQL-014-CREATE-POLICY" || contract_id == "NRSQL-015-ALTER-POLICY") &&
        payload != nullptr) {
        const std::string action = contract_id == "NRSQL-014-CREATE-POLICY" ? "CREATE" : "ALTER";
        std::string table_name;
        if (const auto* table = objectField(*payload, "table")) {
            table_name = renderSchemaPath(*table, resolver, NativeSqlObjectTypeHint::TABLE);
        } else if (const auto* path = objectField(*payload, "path")) {
            table_name = renderSchemaPath(*path, resolver, NativeSqlObjectTypeHint::TABLE);
        }
        std::ostringstream sql;
        sql << action << " POLICY "
            << resolveNameToken(
                   fieldStringAny({"name", "policy_name"}),
                   resolver,
                   NativeSqlObjectTypeHint::POLICY);
        if (!table_name.empty()) {
            sql << " ON " << table_name;
        }
        if (objectField(*payload, "using_expr") != nullptr) {
            sql << " USING (<expr>)";
        }
        if (objectField(*payload, "check_expr") != nullptr) {
            sql << " WITH CHECK (<expr>)";
        }
        out.sql = sql.str();
        return true;
    }

    if (contract_id == "NRSQL-016-DROP-POLICY" && payload != nullptr) {
        std::string table_name;
        if (const auto* table = objectField(*payload, "table")) {
            table_name = renderSchemaPath(*table, resolver, NativeSqlObjectTypeHint::TABLE);
        } else if (const auto* path = objectField(*payload, "path")) {
            table_name = renderSchemaPath(*path, resolver, NativeSqlObjectTypeHint::TABLE);
        }
        uint64_t flags = fieldU64("flags", 0);
        std::string policy_name = fieldString("policy_name");
        if (policy_name.empty()) {
            if (const auto* path = objectField(*payload, "path")) {
                policy_name = renderSchemaPath(*path, resolver, NativeSqlObjectTypeHint::POLICY);
            }
        }
        policy_name = resolveNameToken(policy_name, resolver, NativeSqlObjectTypeHint::POLICY);
        std::ostringstream sql;
        sql << "DROP POLICY ";
        if ((flags & 0x01u) != 0u) {
            sql << "IF EXISTS ";
        }
        sql << policy_name;
        if (!table_name.empty()) {
            sql << " ON " << table_name;
        }
        out.sql = sql.str();
        return true;
    }

    if (contract_id == "NRSQL-017-CREATE-SCHEDULE" && payload != nullptr) {
        std::ostringstream sql;
        sql << "CREATE JOB "
            << fieldStringResolved("name", NativeSqlObjectTypeHint::JOB);
        const std::string schedule = fieldString("schedule");
        if (!schedule.empty()) {
            sql << " SCHEDULE = " << schedule;
        }
        out.sql = sql.str();
        return true;
    }

    if (contract_id == "NRSQL-018-ALTER-SCHEDULE" && payload != nullptr) {
        std::ostringstream sql;
        sql << "ALTER JOB " << fieldStringResolved("job_name", NativeSqlObjectTypeHint::JOB);
        const std::string cron_expr = fieldString("cron_expression");
        const std::string at_timestamp = fieldString("at_timestamp");
        if (!cron_expr.empty()) {
            sql << " SET SCHEDULE = CRON " << sqlQuote(cron_expr);
        } else if (!at_timestamp.empty()) {
            sql << " SET SCHEDULE = AT " << sqlQuote(at_timestamp);
        } else if (objectField(*payload, "interval_seconds") != nullptr) {
            sql << " SET SCHEDULE = EVERY " << fieldU64("interval_seconds") << "s";
        }
        out.sql = sql.str();
        return true;
    }

    if (contract_id == "NRSQL-019-DROP-SCHEDULE" && payload != nullptr) {
        std::string name;
        if (const auto* path = objectField(*payload, "path")) {
            name = renderSchemaPath(*path, resolver, NativeSqlObjectTypeHint::JOB);
        }
        out.sql = "DROP JOB " + name;
        return true;
    }

    out.sql = contract->grammar_signature;
    return true;
}

bool renderNativeSqlInstruction(const v3::Instruction& instruction,
                                NativeSqlRenderResult& out,
                                std::string& error) {
    return renderNativeSqlInstruction(instruction, nullptr, out, error);
}

}  // namespace scratchbird::sblr
