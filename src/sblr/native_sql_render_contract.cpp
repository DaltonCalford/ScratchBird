#include "scratchbird/sblr/native_sql_render_contract.h"

#include <string>
#include <string_view>

#include "scratchbird/sblr/v3_opcode_identity.h"
#include "scratchbird/sblr/v3_opcode_registry.h"

namespace scratchbird::sblr {

namespace {

using v3::Opcode;

constexpr const char* kNoClassifier = "";

constexpr NativeSqlRenderContract kContracts[] = {
    {"NRSQL-001-DOC-PATH-FILTER",
     static_cast<uint16_t>(Opcode::SBLR3_OP_DOC_PATH_FILTER),
     "OP_COMPAT_OP_DOC_PATH_FILTER",
     "DOC PATH FILTER PATH_ID <u64> OP <cmp> VALUE_REF <u64>",
     NativeSqlResultShape::ROWSET_OR_MUTATION,
     kNoClassifier},
    {"NRSQL-002-TS-BUCKET-AGG",
     static_cast<uint16_t>(Opcode::SBLR3_OP_TS_BUCKET_AGG),
     "OP_COMPAT_OP_TS_BUCKET_AGG",
     "TS BUCKET AGG TIME_EXPR <u64> BUCKET_NS <u64> AGG_REFS (<u64,...>)",
     NativeSqlResultShape::ROWSET_OR_MUTATION,
     kNoClassifier},
    {"NRSQL-003-SEARCH-QUERY-DSL",
     static_cast<uint16_t>(Opcode::SBLR3_OP_SEARCH_DSL_EVAL),
     "OP_COMPAT_OP_SEARCH_DSL_EVAL",
     "SEARCH QUERY DSL TARGET_INDEX <u64> PAYLOAD <json> SCORER <id>",
     NativeSqlResultShape::ROWSET_OR_MUTATION,
     kNoClassifier},
    {"NRSQL-004-VECTOR-ANN-QUERY",
     static_cast<uint16_t>(Opcode::SBLR3_OP_VECTOR_ANN),
     "OP_COMPAT_OP_VECTOR_ANN",
     "VECTOR ANN QUERY INDEX <u64> METRIC <id> TOPK <u32> EF_SEARCH <u32>",
     NativeSqlResultShape::ROWSET_OR_MUTATION,
     kNoClassifier},
    {"NRSQL-005-HYBRID-BRIDGE",
     static_cast<uint16_t>(Opcode::SBLR3_OP_HYBRID_BRIDGE_EXCHANGE),
     "OP_COMPAT_OP_HYBRID_BRIDGE_EXCHANGE",
     "HYBRID BRIDGE EXCHANGE SOURCE_TRACK <u64> TARGET_TRACK <u64> MODE <id>",
     NativeSqlResultShape::COMMAND_STATUS,
     kNoClassifier},
    {"NRSQL-006-UDR-COMPILE",
     static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_COMPILE_DISPATCH),
     "OP_COMPAT_OP_UDR_COMPILE_DISPATCH",
     "UDR COMPILE EMBEDDED PAYLOAD PROFILE <id> FORMAT <id> BYTES <id> SESSION_SIGNATURE <id>",
     NativeSqlResultShape::COMMAND_STATUS,
     kNoClassifier},
    {"NRSQL-007-UDR-TEMPLATE",
     static_cast<uint16_t>(Opcode::SBLR3_OP_UDR_EMBEDDED_SQL_COMPILE),
     "OP_COMPAT_OP_UDR_EMBEDDED_SQL_COMPILE",
     "UDR COMPILE SQL TEMPLATE TEMPLATE_ID <id> SQL_TEXT <text> PROFILE <id> SESSION_SIGNATURE <id>",
     NativeSqlResultShape::COMMAND_STATUS,
     kNoClassifier},
    {"NRSQL-010-CREATE-DATABASE-EMULATED",
     static_cast<uint16_t>(Opcode::SBLR3_CREATE_DATABASE),
     "OP_STMT_DDL_CREATE_DATABASE_NATIVE",
     "CREATE DATABASE [EMULATED <engine>] <name>",
     NativeSqlResultShape::COMMAND_STATUS,
     kNoClassifier},
    {"NRSQL-011-CREATE-USER",
     static_cast<uint16_t>(Opcode::SBLR3_CREATE_USER),
     "OP_COMPAT_CREATE_USER",
     "CREATE USER <name> [WITH <options>]",
     NativeSqlResultShape::COMMAND_STATUS,
     kNoClassifier},
    {"NRSQL-012-ALTER-USER",
     static_cast<uint16_t>(Opcode::SBLR3_ALTER_SYSTEM),
     "OP_STMT_CONFIG_SET",
     "ALTER USER <name> [WITH <options>]",
     NativeSqlResultShape::COMMAND_STATUS,
     "security.user.alter."},
    {"NRSQL-013-DROP-USER",
     static_cast<uint16_t>(Opcode::SBLR3_DROP_USER),
     "OP_COMPAT_DROP_USER",
     "DROP USER [IF EXISTS] <name> [CASCADE|RESTRICT]",
     NativeSqlResultShape::COMMAND_STATUS,
     kNoClassifier},
    {"NRSQL-014-CREATE-POLICY",
     static_cast<uint16_t>(Opcode::SBLR3_CREATE_POLICY),
     "OP_STMT_DDL_CREATE_POLICY",
     "CREATE POLICY <name> ON <table> [USING (...)] [WITH CHECK (...)]",
     NativeSqlResultShape::COMMAND_STATUS,
     kNoClassifier},
    {"NRSQL-015-ALTER-POLICY",
     static_cast<uint16_t>(Opcode::SBLR3_ALTER_POLICY),
     "OP_COMPAT_ALTER_POLICY",
     "ALTER POLICY <name> ON <table> [USING (...)] [WITH CHECK (...)]",
     NativeSqlResultShape::COMMAND_STATUS,
     kNoClassifier},
    {"NRSQL-016-DROP-POLICY",
     static_cast<uint16_t>(Opcode::SBLR3_DROP_POLICY),
     "OP_COMPAT_DROP_POLICY",
     "DROP POLICY [IF EXISTS] <name> ON <table>",
     NativeSqlResultShape::COMMAND_STATUS,
     kNoClassifier},
    {"NRSQL-017-CREATE-SCHEDULE",
     static_cast<uint16_t>(Opcode::SBLR3_CREATE_JOB),
     "OP_STMT_JOB_SCHEDULE",
     "CREATE JOB <name> SCHEDULE = ...",
     NativeSqlResultShape::COMMAND_STATUS,
     kNoClassifier},
    {"NRSQL-018-ALTER-SCHEDULE",
     static_cast<uint16_t>(Opcode::SBLR3_ALTER_JOB),
     "OP_STMT_JOB_RETRY",
     "ALTER JOB <name> [SET] SCHEDULE = ...",
     NativeSqlResultShape::COMMAND_STATUS,
     kNoClassifier},
    {"NRSQL-019-DROP-SCHEDULE",
     static_cast<uint16_t>(Opcode::SBLR3_DROP_JOB),
     "OP_COMPAT_DROP_JOB",
     "DROP JOB <name>",
     NativeSqlResultShape::COMMAND_STATUS,
     kNoClassifier},
    {"NRSQL-020-CONNECTION-RULE-CREATE",
     static_cast<uint16_t>(Opcode::SBLR3_ALTER_SYSTEM),
     "OP_STMT_CONFIG_SET",
     "CREATE CONNECTION RULE <name> ...",
     NativeSqlResultShape::COMMAND_STATUS,
     "security.connection_rule.create."},
    {"NRSQL-021-CONNECTION-RULE-ALTER",
     static_cast<uint16_t>(Opcode::SBLR3_ALTER_SYSTEM),
     "OP_STMT_CONFIG_SET",
     "ALTER CONNECTION RULE <name> SET (...) EXPECT VERSION <u64>",
     NativeSqlResultShape::COMMAND_STATUS,
     "security.connection_rule.alter."},
    {"NRSQL-022-CONNECTION-RULE-DROP",
     static_cast<uint16_t>(Opcode::SBLR3_ALTER_SYSTEM),
     "OP_STMT_CONFIG_SET",
     "DROP CONNECTION RULE <name> EXPECT VERSION <u64>",
     NativeSqlResultShape::COMMAND_STATUS,
     "security.connection_rule.drop."},
    {"NRSQL-023-TOKEN-CREATE",
     static_cast<uint16_t>(Opcode::SBLR3_ALTER_SYSTEM),
     "OP_STMT_CONFIG_SET",
     "CREATE TOKEN <name> WITH SCOPE (...)",
     NativeSqlResultShape::COMMAND_STATUS,
     "security.token.create."},
    {"NRSQL-024-TOKEN-ALTER",
     static_cast<uint16_t>(Opcode::SBLR3_ALTER_SYSTEM),
     "OP_STMT_CONFIG_SET",
     "ALTER TOKEN <name> SET (...)",
     NativeSqlResultShape::COMMAND_STATUS,
     "security.token.alter."},
    {"NRSQL-025-TOKEN-REVOKE",
     static_cast<uint16_t>(Opcode::SBLR3_ALTER_SYSTEM),
     "OP_STMT_CONFIG_SET",
     "REVOKE TOKEN <name>",
     NativeSqlResultShape::COMMAND_STATUS,
     "security.token.revoke."},
    {"NRSQL-026-TOKEN-DROP",
     static_cast<uint16_t>(Opcode::SBLR3_ALTER_SYSTEM),
     "OP_STMT_CONFIG_SET",
     "DROP TOKEN <name>",
     NativeSqlResultShape::COMMAND_STATUS,
     "security.token.drop."},
    {"NRSQL-027-QUOTA-PROFILE-CREATE",
     static_cast<uint16_t>(Opcode::SBLR3_ALTER_SYSTEM),
     "OP_STMT_CONFIG_SET",
     "CREATE QUOTA PROFILE <name> (...)",
     NativeSqlResultShape::COMMAND_STATUS,
     "security.quota_profile.create."},
    {"NRSQL-028-QUOTA-PROFILE-ALTER",
     static_cast<uint16_t>(Opcode::SBLR3_ALTER_SYSTEM),
     "OP_STMT_CONFIG_SET",
     "ALTER QUOTA PROFILE <name> SET (...)",
     NativeSqlResultShape::COMMAND_STATUS,
     "security.quota_profile.alter."},
    {"NRSQL-029-QUOTA-PROFILE-DROP",
     static_cast<uint16_t>(Opcode::SBLR3_ALTER_SYSTEM),
     "OP_STMT_CONFIG_SET",
     "DROP QUOTA PROFILE <name>",
     NativeSqlResultShape::COMMAND_STATUS,
     "security.quota_profile.drop."},
    {"NRSQL-030-MEASUREMENT-RETENTION",
     static_cast<uint16_t>(Opcode::SBLR3_ALTER_SYSTEM),
     "OP_STMT_CONFIG_SET",
     "ALTER MEASUREMENT <name> RETENTION <duration>",
     NativeSqlResultShape::COMMAND_STATUS,
     "measurement.retention."},
    {"NRSQL-100-ALTER-SYSTEM-GENERIC",
     static_cast<uint16_t>(Opcode::SBLR3_ALTER_SYSTEM),
     "OP_STMT_CONFIG_SET",
     "ALTER SYSTEM <key> = <value>",
     NativeSqlResultShape::COMMAND_STATUS,
     kNoClassifier},
};

constexpr bool startsWith(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.substr(0, prefix.size()) == prefix;
}

std::string_view instructionClassifierKey(const v3::Instruction& instruction) {
    const auto* obj = std::get_if<v3::Value::Object>(&instruction.payload.data);
    if (obj == nullptr) {
        return {};
    }
    auto key_it = obj->find("key");
    if (key_it == obj->end()) {
        return {};
    }
    const auto* key = std::get_if<std::string>(&key_it->second.data);
    if (key == nullptr) {
        return {};
    }
    return *key;
}

}  // namespace

const NativeSqlRenderContract* nativeSqlRenderContractForInstruction(
    const v3::Instruction& instruction) {
    const std::string_view classifier_key = instructionClassifierKey(instruction);
    const NativeSqlRenderContract* fallback = nullptr;

    for (const auto& contract : kContracts) {
        if (contract.opcode != instruction.opcode) {
            continue;
        }
        if (contract.classifier_key_prefix[0] == '\0') {
            if (fallback == nullptr) {
                fallback = &contract;
            }
            continue;
        }
        if (startsWith(classifier_key, contract.classifier_key_prefix)) {
            return &contract;
        }
    }

    return fallback;
}

const NativeSqlRenderContract* nativeSqlRenderContractForOpcode(uint16_t opcode) {
    for (const auto& contract : kContracts) {
        if (contract.opcode == opcode && contract.classifier_key_prefix[0] == '\0') {
            return &contract;
        }
    }
    return nullptr;
}

const NativeSqlRenderContract* nativeSqlRenderContractTable(size_t& count) {
    count = sizeof(kContracts) / sizeof(kContracts[0]);
    return kContracts;
}

const char* nativeSqlResultShapeName(NativeSqlResultShape shape) {
    switch (shape) {
        case NativeSqlResultShape::COMMAND_STATUS:
            return "RS_COMMAND_STATUS";
        case NativeSqlResultShape::ROWSET_OR_MUTATION:
            return "RS_ROWSET_OR_MUTATION";
        case NativeSqlResultShape::STREAM_STATUS:
            return "RS_STREAM_STATUS";
    }
    return "RS_COMMAND_STATUS";
}

}  // namespace scratchbird::sblr
