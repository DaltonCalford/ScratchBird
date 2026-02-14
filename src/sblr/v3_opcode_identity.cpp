#include "scratchbird/sblr/v3_opcode_identity.h"

#include <string>
#include <string_view>
#include <unordered_map>

#include "scratchbird/sblr/v3_canonical_feature_map.generated.h"
#include "scratchbird/sblr/v3_opcode_registry.h"

namespace scratchbird::sblr::v3 {

namespace {

static const std::unordered_map<std::string_view, std::string_view> kExactSymbolMap = {
    {"SBLR3_VERSION", "OP_MOD_BEGIN"},
    {"SBLR3_END", "OP_MOD_END"},
    {"SBLR3_EXTENDED_OPCODE", "OP_MOD_FEATURE"},

    {"SBLR3_START_TRANSACTION", "OP_STMT_TXN_BEGIN"},
    {"SBLR3_COMMIT", "OP_STMT_TXN_COMMIT"},
    {"SBLR3_ROLLBACK", "OP_STMT_TXN_ROLLBACK"},
    {"SBLR3_SAVEPOINT", "OP_STMT_TXN_SAVEPOINT"},

    {"SBLR3_SELECT", "OP_STMT_DML_SELECT"},
    {"SBLR3_INSERT", "OP_STMT_DML_INSERT"},
    {"SBLR3_UPDATE", "OP_STMT_DML_UPDATE"},
    {"SBLR3_DELETE", "OP_STMT_DML_DELETE"},
    {"SBLR3_MERGE_START", "OP_STMT_DML_MERGE"},

    {"SBLR3_CREATE_TABLE", "OP_STMT_DDL_CREATE_TABLE"},
    {"SBLR3_ALTER_TABLE", "OP_STMT_DDL_ALTER_TABLE"},
    {"SBLR3_DROP_TABLE", "OP_STMT_DDL_DROP_TABLE"},
    {"SBLR3_CREATE_VIEW", "OP_STMT_DDL_CREATE_VIEW"},
    {"SBLR3_CREATE_INDEX", "OP_STMT_DDL_CREATE_INDEX"},
    {"SBLR3_ALTER_INDEX", "OP_STMT_DDL_ALTER_INDEX"},
    {"SBLR3_DROP_INDEX", "OP_STMT_DDL_DROP_INDEX"},
    {"SBLR3_CREATE_DOMAIN", "OP_STMT_DDL_CREATE_DOMAIN"},
    {"SBLR3_ALTER_DOMAIN", "OP_STMT_DDL_ALTER_DOMAIN"},
    {"SBLR3_CREATE_FUNCTION_STMT", "OP_STMT_DDL_CREATE_FUNCTION"},
    {"SBLR3_CREATE_PROCEDURE_STMT", "OP_STMT_DDL_CREATE_PROCEDURE"},
    {"SBLR3_CREATE_PACKAGE_STMT", "OP_STMT_DDL_CREATE_PACKAGE"},
    {"SBLR3_CREATE_TRIGGER", "OP_STMT_DDL_CREATE_TRIGGER"},
    {"SBLR3_CREATE_POLICY", "OP_STMT_DDL_CREATE_POLICY"},
    {"SBLR3_GRANT", "OP_STMT_DDL_GRANT"},
    {"SBLR3_REVOKE", "OP_STMT_DDL_REVOKE"},
};

std::string normalizeExprSuffix(std::string_view suffix) {
    if (suffix == "SUBTRACT") return "SUB";
    if (suffix == "MULTIPLY") return "MUL";
    if (suffix == "DIVIDE") return "DIV";
    if (suffix == "MODULO") return "MOD";
    if (suffix == "NE") return "NEQ";
    if (suffix == "LE") return "LTE";
    if (suffix == "GE") return "GTE";
    return std::string(suffix);
}

std::string mapByPrefix(std::string_view v3_name) {
    constexpr std::string_view kPrefix = "SBLR3_";
    if (v3_name.rfind(kPrefix, 0) != 0) {
        return std::string("OP_COMPAT_") + std::string(v3_name);
    }

    std::string_view suffix = v3_name.substr(kPrefix.size());
    if (suffix.rfind("TYPE_", 0) == 0) {
        return std::string("OP_TYPE_") + std::string(suffix.substr(5));
    }
    if (suffix.rfind("LITERAL_", 0) == 0) {
        return std::string("OP_VAL_") + std::string(suffix.substr(8));
    }
    if (suffix.rfind("EXPR_", 0) == 0) {
        return std::string("OP_EXPR_") + normalizeExprSuffix(suffix.substr(5));
    }
    if (suffix.rfind("PSQL_", 0) == 0) {
        return std::string("OP_FLOW_PSQL_") + std::string(suffix.substr(5));
    }
    if (suffix.rfind("CURSOR_", 0) == 0) {
        return std::string("OP_FLOW_CURSOR_") + std::string(suffix.substr(7));
    }
    if (suffix.rfind("FUNC_", 0) == 0) {
        return std::string("OP_EXPR_FUNC_") + std::string(suffix.substr(5));
    }
    if (suffix.rfind("AGG_", 0) == 0) {
        return std::string("OP_EXPR_AGG_") + std::string(suffix.substr(4));
    }
    if (suffix.rfind("WIN_", 0) == 0) {
        return std::string("OP_EXPR_WIN_") + std::string(suffix.substr(4));
    }
    return std::string("OP_COMPAT_") + std::string(suffix);
}

}  // namespace

std::string canonicalOpcodeSymbolForV3Name(std::string_view v3_name) {
    if (auto it = kExactSymbolMap.find(v3_name); it != kExactSymbolMap.end()) {
        return std::string(it->second);
    }
    return mapByPrefix(v3_name);
}

std::string canonicalOpcodeSymbolForOpcode(uint16_t opcode) {
    const char* name = opcodeName(opcode);
    if (name == nullptr) {
        return "OP_UNKNOWN";
    }
    return canonicalOpcodeSymbolForV3Name(name);
}

bool opcodeMapsToCanonicalFeatureName(std::string_view v3_name) {
    const std::string canonical_symbol = canonicalOpcodeSymbolForV3Name(v3_name);
    return isCanonicalFeatureOpcodeSymbol(canonical_symbol);
}

bool opcodeMapsToCanonicalFeature(uint16_t opcode) {
    const char* name = opcodeName(opcode);
    if (name == nullptr) {
        return false;
    }
    return opcodeMapsToCanonicalFeatureName(name);
}

}  // namespace scratchbird::sblr::v3
